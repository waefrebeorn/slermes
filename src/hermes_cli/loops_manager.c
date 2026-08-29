/*
 * loops_manager.c — faithful C11 port of hermes_cli/loops.py
 * (LoopManager + module-level persistence surface).
 *
 * Split out of loops_pure.c per the no-monolith rule: this file owns the
 * stateful manager; loops_pure.c keeps the pure helpers.
 *
 * Python -> C symbol map (1:1):
 *   _meta_key                  -> (static) loops_meta_key
 *   _loops_config              -> injectable config via loops_pure_set_cfg
 *   load_loop / refresh        -> loops_manager_new / loops_manager_refresh
 *   save_loop                  -> (static) loops_save  [PoP: save_loop]
 *   clear_loop                 -> loops_clear_loop
 *   list_active_loops          -> loops_list_active_loops
 *   migrate_loop_to_session    -> loops_migrate_loop_to_session
 *   _digest_response           -> loops_digest_response (loops_pure.c)
 *   LoopManager.state          -> loops_manager_state
 *   LoopManager.is_active      -> loops_manager_is_active
 *   LoopManager.has_loop       -> loops_manager_has_loop
 *   LoopManager.status_line    -> loops_manager_status_line
 *   LoopManager.set            -> loops_manager_set
 *   LoopManager.pause          -> loops_manager_pause
 *   LoopManager.resume         -> loops_manager_resume
 *   LoopManager.clear          -> loops_manager_clear
 *   LoopManager.mark_done      -> loops_manager_mark_done
 *   LoopManager.is_due         -> loops_manager_is_due / loops_state_is_due
 *   LoopManager.fire_tick      -> loops_manager_fire_tick
 *   LoopManager.abandon_tick   -> loops_manager_abandon_tick
 *   LoopManager.complete_tick  -> loops_manager_complete_tick
 *   goal_blocks_loop_tick      -> loops_goal_blocks_loop_tick
 *   dispatch_loop_command      -> loops_dispatch_loop_command
 */
#define _POSIX_C_SOURCE 200809L
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "loops_pure.h"
#include "loops_state_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* snprintf %f renders "30.000000"; Python json emits "30.0". Match the
 * shortest round-trip form: integral => N.0 like Python's repr. Rotating
 * buffers: several fmt_num calls share one snprintf argument list. */
static const char *fmt_num(double v) {
    static char bufs[8][64];
    static int idx = 0;
    char *buf = bufs[idx++ & 7];
    if (v == (double)(long long)v && v > -1e15 && v < 1e15)
        snprintf(buf, 64, "%.1f", v);
    else
        snprintf(buf, 64, "%.17g", v);
    return buf;
}

/* JSON-string escape for embedding into a serialized object (same rules as
 * the one in loops_pure.c — duplicated here to keep modules self-contained
 * without a god header). */
static void lm_json_escape(const char *s, char *out, size_t outsz) {
    size_t o = 0;
    out[0] = '\0';
    if (!s) return;
    for (size_t i = 0; s[i] && o + 1 < outsz; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  if (o+2 < outsz){out[o++]='\\';out[o++]='"';} break;
            case '\\': if (o+2 < outsz){out[o++]='\\';out[o++]='\\';} break;
            case '\n': if (o+2 < outsz){out[o++]='\\';out[o++]='n';} break;
            case '\r': if (o+2 < outsz){out[o++]='\\';out[o++]='r';} break;
            case '\t': if (o+2 < outsz){out[o++]='\\';out[o++]='t';} break;
            default:
                if (c < 0x20) {
                    if (o+6 < outsz) { snprintf(out+o, outsz-o, "\\u%04x", c); o += 6; }
                } else {
                    out[o++] = (char)c;
                }
        }
    }
    out[o] = '\0';
}

/* Returns malloc'd "escaped-in-quotes", or malloc'd JSON null for NULL. */
static char *lm_quote(const char *s) {
    if (!s) return strdup("null");
    size_t n = strlen(s) * 2 + 8;
    char *out = malloc(n + 2);
    out[0] = '"';
    lm_json_escape(s, out + 1, n + 1);
    strcat(out, "\"");
    return out;
}

/*
 * Canonical config injected by callers/tests.
 * PoP: loops_pure_set_cfg @ hermes_cli/loops.py:_loops_config
 */
static const char *g_cfg_json = NULL;
void loops_pure_set_cfg(const char *cfg) { g_cfg_json = cfg; }

static double lm_self_paced_floor(void) {
    return (double)loops_self_paced_floor_seconds(g_cfg_json);
}
#define LM_SELF_PACED_FLOOR_DEFAULT 60.0

/* Injectable --until judge: returns malloc'd JSON {"verdict","reason"} or
 * NULL when unavailable (fail-open, Python's except path). */
static char *(*loops_goal_judge_hook)(const char *until, const char *response) = NULL;

struct loop_manager {
    char *session_id;
    loop_state_t *state;
    const loops_manager_vtab_t *vtab;
};

/* ──────────────────────────────────────────────────────────────────────
 * Persistence (SessionDB state_meta) — mirrors loops.py's meta layer.
 * ──────────────────────────────────────────────────────────────────── */

/* PoP: ticks_fired @ hermes_cli/loops.py:LoopState.ticks_fired */
long loops_state_ticks_fired(const loop_state_t *s) {
    return s ? s->ticks_fired : 0;
}

/* PoP: awaiting_response @ hermes_cli/loops.py:LoopState.awaiting_response */
bool loops_state_awaiting_response(const loop_state_t *s) {
    return s ? s->awaiting_response : false;
}

/* PoP: loops_state_to_json @ hermes_cli/loops.py:LoopState.to_json */
char *loops_state_to_json(const loop_state_t *s) {
    if (!s) return NULL;
    char pe[8192];
    lm_json_escape(s->prompt ? s->prompt : "", pe, sizeof pe);
    char ue[4096];
    lm_json_escape(s->until ? s->until : "", ue, sizeof ue);
    char de[256];
    lm_json_escape(s->last_response_digest ? s->last_response_digest : "", de,
                   sizeof de);
    char *pq = NULL, *lq = NULL;
    if (s->paused_reason) pq = lm_quote(s->paused_reason);
    if (s->last_stop_reason) lq = lm_quote(s->last_stop_reason);
    size_t cap = strlen(pe) + strlen(ue) + strlen(de) + strlen(s->status) +
                 strlen(s->mode) + 640;
    char *out = malloc(cap);
    /* Python json.dumps(asdict(self), ensure_ascii=False): field order is
     * dataclass declaration order. */
    snprintf(out, cap,
             "{\"prompt\":\"%s\",\"status\":\"%s\",\"mode\":\"%s\","
             "\"interval_seconds\":%s,\"current_delay\":%s,\"times\":%ld,"
             "\"until\":\"%s\",\"max_ticks\":%ld,\"ticks_fired\":%ld,"
             "\"created_at\":%s,\"last_fired_at\":%s,\"next_due_at\":%s,"
             "\"awaiting_response\":%s,\"last_response_digest\":\"%s\","
             "\"route\":{},\"paused_reason\":%s,\"last_stop_reason\":%s}",
             pe, s->status, s->mode,
             fmt_num(s->interval_seconds), fmt_num(s->current_delay), s->times,
             ue, s->max_ticks, s->ticks_fired,
             fmt_num(s->created_at), fmt_num(s->last_fired_at),
             fmt_num(s->next_due_at),
             s->awaiting_response ? "true" : "false", de,
             pq ? pq : "null", lq ? lq : "null");
    free(pq);
    free(lq);
    return out;
}

/* PoP: loops_meta_key @ hermes_cli/loops.py:_meta_key */
static char *loops_meta_key(const char *session_id) {
    if (!session_id) return NULL;
    size_t n = strlen(session_id) + 8;
    char *k = malloc(n);
    snprintf(k, n, "loop:%s", session_id);
    return k;
}

/*

 * PoP: loops_load_loop @ hermes_cli/loops.py:load_loop,refresh
 *
 * LoopManager.__init__ and refresh() both read through this path: the vtab
 * supplies the SessionDB state_meta lookup for "loop:<session_id>". A missing
 * vtab models Python's "_get_session_db() returned None" degradation: the
 * manager simply has no state.
 */
loop_manager_t *loops_manager_new(const char *session_id,
                                  const loops_manager_vtab_t *vtab) {
    loop_manager_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->session_id = strdup(session_id ? session_id : "");
    m->vtab = vtab;
    if (vtab && vtab->load && m->session_id[0]) {
        char *raw = vtab->load(m->session_id);
        if (raw && raw[0]) {
            m->state = loops_state_from_json(raw);
            free(raw);
            if (!m->state)
                fprintf(stderr, "LoopManager: could not parse stored loop for %s\n",
                        m->session_id);
        } else {
            free(raw);
        }
    }
    return m;
}

void loops_manager_free(loop_manager_t *m) {
    if (!m) return;
    free(m->session_id);
    loops_state_free(m->state);
    free(m);
}

const char *loops_manager_session_id(const loop_manager_t *m) {
    return m ? m->session_id : "";
}
/* PoP: loops_manager_state @ hermes_cli/loops.py:LoopManager.state */
loop_state_t *loops_manager_state(loop_manager_t *m) { return m ? m->state : NULL; }

/*
 * Re-read state from the DB (cross-process safety for the gateway).
 * PoP: loops_refresh @ hermes_cli/loops.py:LoopManager.refresh
 */
void loops_manager_refresh(loop_manager_t *m) {
    if (!m) return;
    loops_state_free(m->state);
    m->state = NULL;
    if (m->vtab && m->vtab->load && m->session_id[0]) {
        char *raw = m->vtab->load(m->session_id);
        if (raw && raw[0]) {
            m->state = loops_state_from_json(raw);
            free(raw);
        } else {
            free(raw);
        }
    }
}

/*
 * Persist under "loop:<session_id>". Best-effort: dropped with a warning
 * when the DB layer is unavailable (Python's _warn_dropped_write path).
 * PoP: loops_save @ hermes_cli/loops.py:save_loop
 */
static void loops_save(loop_manager_t *m, const loop_state_t *s) {
    if (!m || !m->session_id || !m->session_id[0] || !s) return;
    if (!m->vtab || !m->vtab->save) {
        fprintf(stderr,
                "LoopManager: DB unavailable, dropped /loop write for %s\n",
                m->session_id);
        return;
    }
    char *js = loops_state_to_json(s);
    if (js) {
        m->vtab->save(m->session_id, js);
        free(js);
    }
}

/* PoP: loops_manager_is_active @ hermes_cli/loops.py:LoopManager.is_active */
int loops_manager_is_active(const loop_manager_t *m) {
    return m && m->state && strcmp(m->state->status, "active") == 0;
}
/* PoP: loops_manager_has_loop @ hermes_cli/loops.py:LoopManager.has_loop */
int loops_manager_has_loop(const loop_manager_t *m) {
    return m && m->state && (strcmp(m->state->status, "active") == 0 ||
                             strcmp(m->state->status, "paused") == 0);
}

/*
 * Same rendering as loops_state_status_line but on the live manager state.
 * PoP: loops_manager_status_line @ hermes_cli/loops.py:LoopManager.status_line
 */
char *loops_manager_status_line(loop_manager_t *m, double now) {
    if (!m)
        return strdup("No loop set. Start one with /loop [interval] <prompt>.");
    return loops_state_status_line(m->state, now);
}

/*
 * Start a new loop (replaces any existing one for the session).
 * PoP: loops_manager_set @ hermes_cli/loops.py:LoopManager.set
 */
loop_state_t *loops_manager_set(loop_manager_t *m, const char *prompt,
                                int interval_seconds, long times,
                                const char *until, const char *route_json,
                                double now) {
    if (!m) return NULL;
    char p[8192];
    snprintf(p, sizeof p, "%s", prompt ? prompt : "");
    size_t l = strlen(p);
    while (l > 0 && isspace((unsigned char)p[l - 1])) p[--l] = '\0';
    char *s2 = p;
    while (*s2 && isspace((unsigned char)*s2)) s2++;
    l = strlen(s2);
    if (l == 0) {
        fprintf(stderr, "LoopManager.set: loop prompt is empty\n");
        return NULL;
    }

    loop_state_t *st = calloc(1, sizeof(*st));
    st->prompt = strdup(s2);
    st->status = strdup("active");
    double interval;
    if (interval_seconds > 0) {
        interval = (double)(interval_seconds > loops_min_interval_seconds(g_cfg_json)
                                ? interval_seconds
                                : loops_min_interval_seconds(g_cfg_json));
        st->mode = strdup("interval");
        st->interval_seconds = interval;
        st->current_delay = interval;
        st->next_due_at = now + interval;
    } else {
        interval = lm_self_paced_floor();
        st->mode = strdup("self_paced");
        st->interval_seconds = 0.0;
        st->current_delay = interval;
        st->next_due_at = now + interval;
    }
    st->times = times > 0 ? times : 0;
    if (until && until[0]) {
        char *u = strdup(until);
        size_t ul = strlen(u);
        while (ul > 0 && isspace((unsigned char)u[ul - 1])) u[--ul] = '\0';
        char *u2 = u;
        while (*u2 && isspace((unsigned char)*u2)) u2++;
        st->until = strdup(u2);
        free(u);
    } else {
        st->until = strdup("");
    }
    st->max_ticks = loops_max_ticks_default(g_cfg_json);
    st->created_at = now;
    (void)route_json; /* route stored verbatim by gateway-routing callers */
    loops_state_free(m->state);
    m->state = st;
    loops_save(m, st);
    return st;
}

/* PoP: loops_manager_pause @ hermes_cli/loops.py:LoopManager.pause */
loop_state_t *loops_manager_pause(loop_manager_t *m, const char *reason) {
    if (!m || !m->state ||
        (strcmp(m->state->status, "active") != 0 &&
         strcmp(m->state->status, "paused") != 0))
        return NULL;
    free(m->state->status);
    m->state->status = strdup("paused");
    free(m->state->paused_reason);
    m->state->paused_reason = strdup(reason ? reason : "user-paused");
    m->state->awaiting_response = 0;
    loops_save(m, m->state);
    return m->state;
}

/*
 * Re-arm relative to now so a long pause doesn't fire instantly N times.
 * PoP: loops_manager_resume @ hermes_cli/loops.py:LoopManager.resume
 */
loop_state_t *loops_manager_resume(loop_manager_t *m, double now,
                                   double self_paced_floor) {
    if (!m || !m->state || strcmp(m->state->status, "cleared") == 0)
        return NULL;
    free(m->state->status);
    m->state->status = strdup("active");
    free(m->state->paused_reason);
    m->state->paused_reason = NULL;
    m->state->awaiting_response = 0;
    double delay = m->state->current_delay > 0 ? m->state->current_delay
                 : (m->state->interval_seconds > 0 ? m->state->interval_seconds
                                                   : self_paced_floor);
    if (delay <= 0) delay = self_paced_floor;
    m->state->next_due_at = now + (delay < 5.0 ? delay : 5.0);
    loops_save(m, m->state);
    return m->state;
}

/* PoP: loops_manager_clear @ hermes_cli/loops.py:LoopManager.clear */
int loops_manager_clear(loop_manager_t *m) {
    if (!m || !m->state || strcmp(m->state->status, "cleared") == 0)
        return 0;
    free(m->state->status);
    m->state->status = strdup("cleared");
    loops_save(m, m->state);
    loops_state_free(m->state);
    m->state = NULL;
    return 1;
}

/* PoP: loops_manager_mark_done @ hermes_cli/loops.py:LoopManager.mark_done */
void loops_manager_mark_done(loop_manager_t *m, const char *reason) {
    if (!m || !m->state) return;
    free(m->state->status);
    m->state->status = strdup("done");
    free(m->state->last_stop_reason);
    m->state->last_stop_reason = strdup(reason ? reason : "");
    m->state->awaiting_response = 0;
    loops_save(m, m->state);
}

/*
 * Claim a due tick. Returns the wakeup message (or raw slash command) to
 * inject, or NULL when not due. Marks awaiting_response so a tick can't
 * double-fire while its turn runs; drivers MUST follow up with
 * complete_tick (or abandon_tick on injection failure). Provisionally
 * schedules the next tick from NOW so a process death mid-turn can't leave
 * the persisted loop 'due' in a tight loop.
 * PoP: loops_manager_fire_tick @ hermes_cli/loops.py:LoopManager.fire_tick
 */
char *loops_manager_fire_tick(loop_manager_t *m, double now,
                              double self_paced_floor) {
    loop_state_t *s = m ? m->state : NULL;
    if (!s || !loops_state_is_due(s, now)) return NULL;
    s->ticks_fired += 1;
    s->last_fired_at = now;
    s->awaiting_response = 1;
    double delay = s->current_delay > 0 ? s->current_delay
                 : (s->interval_seconds > 0 ? s->interval_seconds
                                            : self_paced_floor);
    s->next_due_at = s->last_fired_at + delay;
    loops_save(m, s);

    /* Slash-command prompts pass through raw so normal dispatch handles them. */
    const char *p = s->prompt;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '/') {
        char *pt = strdup(p);
        size_t pl = strlen(pt);
        while (pl > 0 && isspace((unsigned char)pt[pl - 1])) pt[--pl] = '\0';
        return pt;
    }

    char *cadence;
    if (strcmp(s->mode, "interval") == 0) {
        char *cl = loops_state_cadence_label(s);
        cadence = malloc(strlen(cl) + 4);
        sprintf(cadence, ", %s", cl);
        free(cl);
    } else {
        cadence = strdup(", self-paced");
    }

    /* WAKEUP_PROMPT_WITH_UNTIL_TEMPLATE / WAKEUP_PROMPT_TEMPLATE */
    char *out;
    if (s->until && s->until[0]) {
        size_t need = strlen(s->prompt) * 2 + strlen(cadence) +
                      strlen(s->until) + 1024;
        out = malloc(need);
        snprintf(out, need,
            "[/loop wakeup #%ld%s]\nRecurring task: %s\n\nStop condition: %s\n\n"
            "This is an automatic wakeup from the /loop the user set. Perform the "
            "task now against the CURRENT state (re-check files, processes, or "
            "services fresh \xE2\x80\x94 do not assume anything from earlier iterations still "
            "holds). Report concisely what you found or did this iteration, and "
            "show concrete evidence of the stop condition's status.\n"
            "If the stop condition is met, or the task is no longer applicable, say "
            "so and end your reply with LOOP_COMPLETE on its own line \xE2\x80\x94 "
            "that stops the loop.",
            s->ticks_fired, cadence, s->prompt, s->until);
    } else {
        size_t need = strlen(s->prompt) * 2 + strlen(cadence) + 1024;
        out = malloc(need);
        snprintf(out, need,
            "[/loop wakeup #%ld%s]\nRecurring task: %s\n\n"
            "This is an automatic wakeup from the /loop the user set. Perform the "
            "task now against the CURRENT state (re-check files, processes, or "
            "services fresh \xE2\x80\x94 do not assume anything from earlier iterations still "
            "holds). Report concisely what you found or did this iteration.\n"
            "If the task is now complete, no longer applicable, or the thing you "
            "were watching has finished, say so and end your reply with "
            "LOOP_COMPLETE on its own line \xE2\x80\x94 that stops the loop.",
            s->ticks_fired, cadence, s->prompt);
    }
    free(cadence);
    return out;
}

/*
 * Roll back a fired tick whose injection failed (nothing ran).
 * PoP: loops_manager_abandon_tick @ hermes_cli/loops.py:LoopManager.abandon_tick
 */
void loops_manager_abandon_tick(loop_manager_t *m) {
    loop_state_t *s = m ? m->state : NULL;
    if (!s || !s->awaiting_response) return;
    s->awaiting_response = 0;
    s->ticks_fired = s->ticks_fired > 0 ? s->ticks_fired - 1 : 0;
    loops_save(m, s);
}

/*
 * Evaluate the finished wakeup turn and schedule what's next. Returns a
 * malloc'd decision JSON {"status","stopped","reason","message"}:
 *   1. agent LOOP_COMPLETE marker  -> done
 *   2. --until judge (injectable, fail-open) -> done
 *   3. --times user cap -> done
 *   4. config max_ticks budget -> paused (recoverable)
 *   5. otherwise schedule next tick from turn end (self-paced backoff on
 *      unchanged digest), stay active.
 * PoP: loops_manager_complete_tick @ hermes_cli/loops.py:LoopManager.complete_tick
 */
char *loops_manager_complete_tick(loop_manager_t *m, const char *last_response,
                                  double now, double self_paced_floor,
                                  double self_paced_ceiling) {
    loop_state_t *s = m ? m->state : NULL;
    if (!s || !s->awaiting_response) {
        char *rq = lm_quote(s ? s->status : NULL);
        char *out = malloc(strlen(rq) + 128);
        snprintf(out, strlen(rq) + 128,
                 "{\"status\":%s,\"stopped\":false,"
                 "\"reason\":\"no tick in flight\",\"message\":\"\"}",
                 rq);
        free(rq);
        return out;
    }
    s->awaiting_response = 0;

#define LM_EMIT(status_v, stopped_v, reason_v, msg_v)                          \
    do {                                                                       \
        char *_rq = lm_quote(reason_v);                                        \
        char *_mq = lm_quote(msg_v);                                           \
        char *_out = malloc(strlen(_rq) + strlen(_mq) + 96);                   \
        snprintf(_out, strlen(_rq) + strlen(_mq) + 96,                         \
                 "{\"status\":\"%s\",\"stopped\":%s,\"reason\":%s,"            \
                 "\"message\":%s}",                                             \
                 status_v, stopped_v ? "true" : "false", _rq, _mq);             \
        free(_rq);                                                             \
        free(_mq);                                                             \
        loops_save(m, s);                                                      \
        return _out;                                                           \
    } while (0)

    /* 1. Agent self-stop marker. */
    if (loops_response_signals_complete(last_response)) {
        free(s->status);
        s->status = strdup("done");
        free(s->last_stop_reason);
        s->last_stop_reason = strdup("agent signaled the task is complete");
        char msg[192];
        snprintf(msg, sizeof msg,
                 "\xE2\x9C\x93 Loop finished after %ld tick%s \xE2\x80\x94 task complete.",
                 s->ticks_fired, s->ticks_fired != 1 ? "s" : "");
        LM_EMIT("done", 1, s->last_stop_reason, msg);
    }

    /* 2. Evidence-based --until judge (reuses the /goal judge; fail-open). */
    if (s->until && s->until[0] && last_response &&
        last_response[strspn(last_response, " \t\r\n")]) {
        const char *verdict = "continue";
        char reason_dyn[512];
        snprintf(reason_dyn, sizeof reason_dyn, "%s",
                 "judge unavailable: hook missing");
        if (loops_goal_judge_hook) {
            char *jr = loops_goal_judge_hook(s->until, last_response);
            if (jr) {
                json_t *j = json_parse(jr, NULL);
                if (j) {
                    const char *v = json_get_str(j, "verdict", "continue");
                    const char *r = json_get_str(j, "reason", "");
                    verdict = (strcmp(v, "done") == 0) ? "done" : "continue";
                    snprintf(reason_dyn, sizeof reason_dyn, "%s", r);
                    json_free(j);
                }
                free(jr);
            }
        }
        if (strcmp(verdict, "done") == 0) {
            free(s->status);
            s->status = strdup("done");
            char rs[640];
            snprintf(rs, sizeof rs, "stop condition met: %s", reason_dyn);
            free(s->last_stop_reason);
            s->last_stop_reason = strdup(rs);
            char msg[256];
            snprintf(msg, sizeof msg,
                     "\xE2\x9C\x93 Loop finished after %ld tick%s \xE2\x80\x94 %s",
                     s->ticks_fired, s->ticks_fired != 1 ? "s" : "", reason_dyn);
            LM_EMIT("done", 1, s->last_stop_reason, msg);
        }
    }

    /* 3. --times user cap. */
    if (s->times && s->ticks_fired >= s->times) {
        free(s->status);
        s->status = strdup("done");
        char rs[128], msg[160];
        snprintf(rs, sizeof rs, "completed the requested %ld runs", s->times);
        free(s->last_stop_reason);
        s->last_stop_reason = strdup(rs);
        snprintf(msg, sizeof msg,
                 "\xE2\x9C\x93 Loop finished \xE2\x80\x94 ran %ld/%ld times.",
                 s->times, s->times);
        LM_EMIT("done", 1, rs, msg);
    }

    /* 4. Config backstop budget → pause (recoverable), not done. */
    if (s->max_ticks && s->ticks_fired >= s->max_ticks) {
        free(s->status);
        s->status = strdup("paused");
        char rs[160];
        snprintf(rs, sizeof rs, "tick budget exhausted (%ld/%ld)",
                 s->ticks_fired, s->max_ticks);
        free(s->paused_reason);
        s->paused_reason = strdup(rs);
        char msg[288];
        snprintf(msg, sizeof msg,
                 "\xE2\x8F\xB8 Loop paused \xE2\x80\x94 %ld/%ld ticks used "
                 "(loops.max_ticks). /loop resume to keep going, /loop stop to end it.",
                 s->ticks_fired, s->max_ticks);
        LM_EMIT("paused", 1, rs, msg);
    }

    /* 5. Still looping — schedule the next tick from turn end. Self-paced
     * backs off when the response digest is unchanged (nothing changed). */
    if (strcmp(s->mode, "self_paced") == 0) {
        char *digest = loops_digest_response(last_response);
        int unchanged = digest && digest[0] && s->last_response_digest &&
                        strcmp(digest, s->last_response_digest) == 0;
        if (unchanged) {
            double d = s->current_delay < self_paced_floor ? self_paced_floor
                                                           : s->current_delay;
            d *= 2;
            s->current_delay = d > self_paced_ceiling ? self_paced_ceiling : d;
        } else {
            s->current_delay = self_paced_floor;
        }
        free(s->last_response_digest);
        s->last_response_digest = strdup(digest ? digest : "");
        free(digest);
    } else {
        s->current_delay = s->interval_seconds;
    }
    s->next_due_at = now + s->current_delay;
    LM_EMIT("active", 0, "loop continues", "");
#undef LM_EMIT
}

/*
 * Cheap check: active, not mid-wakeup, and the clock has passed.
 * PoP: loops_manager_is_due @ hermes_cli/loops.py:LoopManager.is_due
 */
int loops_manager_is_due(loop_manager_t *m, double now) {
    return loops_state_is_due(m ? m->state : NULL, now);
}

/* PoP: loops_goal_hooks @ hermes_cli/goals.py (injectable GoalManager seam) */
loops_goal_hooks_t loops_goal_hooks = { NULL, NULL };

/*
 * True when an ACTIVE /goal should defer this session's /loop tick. A goal
 * parked on a wait barrier does NOT block the loop. Any probe failure is
 * Python's except path: False (never blocks).
 * PoP: loops_goal_blocks_loop_tick @ hermes_cli/loops.py:goal_blocks_loop_tick
 */
int loops_goal_blocks_loop_tick(const char *session_id) {
    if (!session_id || !session_id[0]) return 0;
    if (!loops_goal_hooks.goal_is_active) return 0;
    if (!loops_goal_hooks.goal_is_active(session_id)) return 0;
    if (!loops_goal_hooks.goal_is_waiting) return 1;
    return loops_goal_hooks.goal_is_waiting(session_id) ? 0 : 1;
}

/*
 * Mark a loop cleared in the DB (preserved for audit, status=cleared).
 * PoP: loops_clear_loop @ hermes_cli/loops.py:clear_loop
 */
int loops_clear_loop(const char *session_id, const loops_manager_vtab_t *vtab) {
    if (!session_id || !session_id[0] || !vtab || !vtab->load || !vtab->save)
        return 0;
    char *key = loops_meta_key(session_id);
    if (!key) return 0;
    free(key);
    char *raw = vtab->load(session_id);
    if (!raw || !raw[0]) { free(raw); return 0; }
    loop_state_t *st = loops_state_from_json(raw);
    free(raw);
    if (!st) return 0;
    if (strcmp(st->status, "cleared") == 0) { loops_state_free(st); return 0; }
    free(st->status);
    st->status = strdup("cleared");
    char *js = loops_state_to_json(st);
    if (js) { vtab->save(session_id, js); free(js); }
    loops_state_free(st);
    return 1;
}

/*
 * [(session_id, LoopState)] for every ACTIVE loop — used by the gateway's
 * idle wakeup watcher. Returns malloc'd JSON array
 * [{"session_id":"...","state":{...}}, ...]; NULL when the DB layer is
 * unavailable (Python returns []).
 * PoP: loops_list_active_loops @ hermes_cli/loops.py:list_active_loops
 */
char *loops_list_active_loops(const char *prefix,
                              const loops_manager_vtab_t *vtab) {
    if (!prefix || !vtab || !vtab->list_prefix) return NULL;
    char *raw = vtab->list_prefix(prefix);
    if (!raw) return NULL;
    json_t *arr = json_parse(raw, NULL);
    free(raw);
    if (!arr || arr->type != JSON_ARRAY) {
        if (arr) json_free(arr);
        return NULL;
    }
    size_t cap = 64;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        cap += strlen(json_get_str(e, "session_id", "")) +
               strlen(json_get_str(e, "json", "")) + 64;
    }
    char *out = malloc(cap);
    size_t o = 0;
    out[o++] = '[';
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        const char *sid = json_get_str(e, "session_id", "");
        const char *js = json_get_str(e, "json", "");
        loop_state_t *st = loops_state_from_json(js);
        if (!st || strcmp(st->status, "active") != 0) {
            loops_state_free(st);
            continue;
        }
        char *sj = loops_state_to_json(st);
        loops_state_free(st);
        if (o > 1) out[o++] = ',';
        char sid_esc[1024], sj_esc_cap_note[1];
        (void)sj_esc_cap_note;
        lm_json_escape(sid, sid_esc, sizeof sid_esc);
        /* sj is itself JSON — embed verbatim as a nested value. */
        o += (size_t)snprintf(out + o, cap - o,
                              "{\"session_id\":\"%s\",\"state\":%s}",
                              sid_esc, sj ? sj : "null");
        free(sj);
    }
    out[o++] = ']';
    out[o] = '\0';
    json_free(arr);
    return out;
}

/*
 * Carry a persistent /loop from a parent session to its continuation at the
 * context-compaction boundary (#33618): copies the loop onto the new session
 * and archives the old row as cleared so exactly one active loop row exists
 * per logical conversation. Best-effort; never fails loudly.
 * PoP: loops_migrate_loop_to_session @ hermes_cli/loops.py:migrate_loop_to_session
 */
int loops_migrate_loop_to_session(const char *old_sid, const char *new_sid,
                                  const loops_manager_vtab_t *vtab) {
    if (!old_sid || !new_sid || !old_sid[0] || !new_sid[0] ||
        strcmp(old_sid, new_sid) == 0)
        return 0;
    if (!vtab || !vtab->load || !vtab->save) return 0;
    char *old_raw = vtab->load(old_sid);
    if (!old_raw || !old_raw[0]) { free(old_raw); return 0; }
    loop_state_t *st = loops_state_from_json(old_raw);
    free(old_raw);
    if (!st) return 0;
    if (strcmp(st->status, "cleared") == 0) { loops_state_free(st); return 0; }
    char *existing = vtab->load(new_sid);
    if (existing && existing[0]) {
        free(existing);
        loops_state_free(st);
        return 0;
    }
    free(existing);

    char *js = loops_state_to_json(st);
    if (js) { vtab->save(new_sid, js); free(js); }
    free(st->status);
    st->status = strdup("cleared");
    char *cj = loops_state_to_json(st);
    if (cj) { vtab->save(old_sid, cj); free(cj); }
    loops_state_free(st);
    return 1;
}

/*
 * Surface-agnostic handler for "/loop <args>" shared by CLI + gateway + TUI.
 * Returns malloc'd JSON {"output": str, "created": bool}. `now` and the
 * cadence knobs are injected so surfaces get deterministic output.
 * PoP: loops_dispatch_loop_command @ hermes_cli/loops.py:dispatch_loop_command
 */
char *loops_dispatch_loop_command(loop_manager_t *mgr, const char *args,
                                  const char *route_json, double now,
                                  int min_interval, int max_ticks_cfg,
                                  double sp_floor, double sp_ceiling) {
    (void)route_json; (void)min_interval; (void)max_ticks_cfg;
    (void)sp_floor;   (void)sp_ceiling;
    char arg[8192];
    snprintf(arg, sizeof arg, "%s", args ? args : "");
    size_t l = strlen(arg);
    while (l > 0 && isspace((unsigned char)arg[l - 1])) arg[--l] = '\0';
    char lower[8192];
    snprintf(lower, sizeof lower, "%s", arg);
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);

    const char *output;
    int created = 0;
    char *heap_out = NULL;

    if (arg[0] == '\0' || strcmp(lower, "status") == 0) {
        heap_out = loops_manager_status_line(mgr, now);
        output = heap_out;
        goto emit;
    }
    if (strcmp(lower, "pause") == 0) {
        loop_state_t *st = loops_manager_pause(mgr, "user-paused");
        if (!st) { output = "No loop set."; goto emit; }
        size_t need = strlen(st->prompt) + 64;
        heap_out = malloc(need);
        snprintf(heap_out, need,
                 "\xE2\x8F\xB8 Loop paused: %s\nUse /loop resume to continue.",
                 st->prompt);
        output = heap_out;
        goto emit;
    }
    if (strcmp(lower, "resume") == 0) {
        loop_state_t *st =
            loops_manager_resume(mgr, now, LM_SELF_PACED_FLOOR_DEFAULT);
        if (!st) { output = "No loop to resume."; goto emit; }
        char *cad = loops_state_cadence_label(st);
        size_t need = strlen(cad) + strlen(st->prompt) + 32;
        heap_out = malloc(need);
        snprintf(heap_out, need, "\xE2\x96\xB6 Loop resumed (%s): %s",
                 cad, st->prompt);
        free(cad);
        output = heap_out;
        goto emit;
    }
    if (strcmp(lower, "stop") == 0 || strcmp(lower, "clear") == 0 ||
        strcmp(lower, "cancel") == 0) {
        int had = loops_manager_clear(mgr);
        output = had ? "\xE2\x9C\x93 Loop stopped." : "No active loop.";
        goto emit;
    }
    if (strcmp(lower, "help") == 0 || strcmp(lower, "--help") == 0 ||
        strcmp(lower, "-h") == 0) {
        output =
            "Usage: /loop [interval] <prompt> [--times N] [--until <condition>]\n"
            "  /loop 5m check the deploy status      \xE2\x80\x94 fixed cadence\n"
            "  /loop every 10m /recap                \xE2\x80\x94 loop a slash command\n"
            "  /loop keep fixing tests until green   \xE2\x80\x94 self-paced (backs off while output is unchanged)\n"
            "  /loop 2m poll CI --times 30           \xE2\x80\x94 stop after 30 runs\n"
            "  /loop 5m watch the queue --until queue is empty\n"
            "Controls: /loop status \xC2\xB7 /loop pause \xC2\xB7 /loop resume \xC2\xB7 /loop stop\n"
            "The loop also stops itself when the agent replies with "
            "LOOP_COMPLETE.";
        goto emit;
    }

    {
        char *pj = loops_parse_loop_args(arg);
        json_t *parsed = pj ? json_parse(pj, NULL) : NULL;
        free(pj);
        const char *err = parsed ? json_get_str(parsed, "error", "") : "";
        if (err && err[0]) {
            if (strcmp(err, "empty") == 0) {
                output =
                    "Usage: /loop [interval] <prompt> \xE2\x80\x94 see /loop help.";
            } else {
                heap_out = malloc(strlen(err) + 16);
                snprintf(heap_out, strlen(err) + 16, "/loop: %s", err);
                output = heap_out;
            }
            if (parsed) json_free(parsed);
            goto emit;
        }
        const char *pprompt = json_get_str(parsed, "prompt", "");
        long iv = (long)json_get_num(parsed, "interval_seconds", -1.0);
        long ptimes = (long)json_get_num(parsed, "times", 0.0);
        const char *puntil = json_get_str(parsed, "until", "");

        int replacing = loops_manager_has_loop(mgr);
        loop_state_t *st =
            loops_manager_set(mgr, pprompt, iv > 0 ? (int)iv : 0, ptimes,
                              puntil, route_json, now);
        if (parsed) json_free(parsed);
        if (!st) { output = "/loop: loop prompt is empty"; goto emit; }

        char *cad = loops_state_cadence_label(st);
        char *rem = loops_state_remaining_label(st, now);
        size_t need = strlen(cad) + strlen(st->prompt) * 2 +
                      strlen(rem) + (st->until ? strlen(st->until) : 0) + 1024;
        char *body = malloc(need);
        snprintf(body, need, "\xE2\x86\xBB Loop set (%s): %s", cad, st->prompt);
        if (iv > 0 && (double)iv < st->interval_seconds) {
            char *mi = loops_format_interval(st->interval_seconds);
            char ln[192];
            snprintf(ln, sizeof ln,
                     "(interval raised to the %s minimum \xE2\x80\x94 "
                     "loops.min_interval_seconds)", mi);
            strcat(body, "\n");
            strcat(body, ln);
            free(mi);
        }
        if (strcmp(st->mode, "self_paced") == 0) {
            char *fd = loops_format_interval(st->current_delay);
            char *cd = loops_format_interval(sp_ceiling);
            char ln[256];
            snprintf(ln, sizeof ln,
                "Self-paced: first check in %s; backs off up to %s while nothing changes.",
                fd, cd);
            strcat(body, "\n");
            strcat(body, ln);
            free(fd);
            free(cd);
        }
        if (st->times) {
            char ln[96];
            snprintf(ln, sizeof ln, "Runs %ld time%s, then stops.",
                     st->times, st->times != 1 ? "s" : "");
            strcat(body, "\n");
            strcat(body, ln);
        }
        if (st->until && st->until[0]) {
            char ln[512];
            snprintf(ln, sizeof ln, "Stops when: %s", st->until);
            strcat(body, "\n");
            strcat(body, ln);
        }
        if (!st->times && st->max_ticks) {
            char ln[128];
            snprintf(ln, sizeof ln,
                     "Backstop budget: %ld ticks (loops.max_ticks; 0 = unlimited).",
                     st->max_ticks);
            strcat(body, "\n");
            strcat(body, ln);
        }
        {
            char ln[192];
            snprintf(ln, sizeof ln,
                     "First wakeup %s. Controls: /loop status \xC2\xB7 pause \xC2\xB7 resume \xC2\xB7 stop.",
                     rem);
            strcat(body, "\n");
            strcat(body, ln);
        }
        if (replacing) {
            /* lines.insert(1, "(replaced the previous loop for this session)") */
            const char *nl = strchr(body, '\n');
            size_t head = nl ? (size_t)(nl - body) : strlen(body);
            char *nb = malloc(strlen(body) + 64);
            memcpy(nb, body, head);
            nb[head] = '\0';
            strcat(nb, "\n(replaced the previous loop for this session)");
            strcat(nb, body + head);
            free(body);
            body = nb;
        }
        free(cad);
        free(rem);
        heap_out = body;
        output = heap_out;
        created = 1;
    }

emit: {
        char oe[16384];
        lm_json_escape(output ? output : "", oe, sizeof oe);
        size_t cap = strlen(oe) + 48;
        char *out = malloc(cap);
        snprintf(out, cap, "{\"output\":\"%s\",\"created\":%s}", oe,
                 created ? "true" : "false");
        free(heap_out);
        return out;
    }
}
