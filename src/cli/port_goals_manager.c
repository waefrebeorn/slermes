/*
 * port_goals_manager.c — GoalManager + judge-prompt helpers (pure layer).
 *
 * Faithful C port of the orchestration + text-shaping slice of
 * hermes_cli/goals.py. Covers the data-layer surface of GoalManager plus the
 * pure judge helpers. The LLM call (judge_goal / draft_contract), the wait
 * barriers' OS-process liveness (_pid_alive / _session_waiting), and
 * evaluate_after_turn / run_kanban_goal_loop (which drive the judge + run_turn)
 * are intentionally NOT implemented here — they require network/LLM/process
 * dependencies. Process liveness + persistence are injected via a vtable so
 * this module stays pure (no DB, TTY, network, or OS-process calls).
 *
/* PoP: goal_manager_set @ hermes_cli/goals.py:set */
/* PoP: goal_manager_set_contract @ hermes_cli/goals.py:set_contract */
/* PoP: goal_manager_pause @ hermes_cli/goals.py:pause */
/* PoP: goal_manager_resume @ hermes_cli/goals.py:resume */
/* PoP: goal_manager_clear @ hermes_cli/goals.py:clear */
/* PoP: goal_manager_mark_done @ hermes_cli/goals.py:mark_done */
/* PoP: goal_manager_add_subgoal @ hermes_cli/goals.py:add_subgoal */
/* PoP: goal_manager_remove_subgoal @ hermes_cli/goals.py:remove_subgoal */
/* PoP: goal_manager_clear_subgoals @ hermes_cli/goals.py:clear_subgoals */
/* PoP: goal_manager_status_line @ hermes_cli/goals.py:status_line */
/* PoP: goal_manager_has_goal @ hermes_cli/goals.py:has_goal */
/* PoP: goal_manager_has_contract @ hermes_cli/goals.py:has_contract */
/* PoP: goal_manager_is_active @ hermes_cli/goals.py:is_active */
/* PoP: goal_manager_is_waiting @ hermes_cli/goals.py:is_waiting */
/* PoP: goal_manager_stop_waiting @ hermes_cli/goals.py:stop_waiting */
/* PoP: goal_manager_wait_on @ hermes_cli/goals.py:wait_on */
/* PoP: goal_manager_wait_on_session @ hermes_cli/goals.py:wait_on_session */
/* PoP: goal_manager_wait_for_seconds @ hermes_cli/goals.py:wait_for_seconds */
/* PoP: goal_manager_next_continuation_prompt @ hermes_cli/goals.py:next_continuation_prompt */
/* PoP: goal_manager_render_contract @ hermes_cli/goals.py:render_contract */
/* PoP: goal_manager_render_subgoals @ hermes_cli/goals.py:render_subgoals */
/* PoP: goal_judge_extract_json @ hermes_cli/goals.py:_extract_json_object */
/* PoP: goal_judge_parse_response @ hermes_cli/goals.py:_parse_judge_response */
/* PoP: goal_judge_render_background_block @ hermes_cli/goals.py:_render_background_block */
/* PoP: goal_judge_max_tokens_resolved @ hermes_cli/goals.py:_goal_judge_max_tokens */


#include "goal_contract.h"
#include "goal_contract_internal.h"
#include "tools/process_registry.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#include "libjson/json.h"

/* canonical contract field name by index (data.c owns N_CONTRACT_FIELDS) */
#define CONTRACT_FIELD_NAME(i) ( \
    (i)==0 ? "outcome" : (i)==1 ? "verification" : (i)==2 ? "constraints" : \
    (i)==3 ? "boundaries" : "stop_when")

/* truncate a string to max chars (replacing tail with '… [truncated]' semantics
 * is skipped here — caller only needs the leading max chars). */
static void truncate_copy(const char *src, char *dst, size_t max) {
    size_t n = src ? strlen(src) : 0;
    if (n > max) n = max;
    memcpy(dst, src ? src : "", n);
    dst[n] = '\0';
}

/* ── GoalState rich accessors ─────────────────────────────────────── */

static goal_status_t status_from_str(const char *s) {
    if (!s) return GOAL_STATUS_ACTIVE;
    if (strcmp(s, "paused") == 0)  return GOAL_STATUS_PAUSED;
    if (strcmp(s, "done") == 0)    return GOAL_STATUS_DONE;
    if (strcmp(s, "cleared") == 0) return GOAL_STATUS_CLEARED;
    return GOAL_STATUS_ACTIVE;
}
static const char *status_to_str(goal_status_t st) {
    switch (st) {
        case GOAL_STATUS_PAUSED:  return "paused";
        case GOAL_STATUS_DONE:    return "done";
        case GOAL_STATUS_CLEARED: return "cleared";
        default:                  return "active";
    }
}

void goal_state_set_contract(goal_state_t *s, goal_contract_t *c) {
    if (!s) return;
    if (s->contract) goal_contract_free(s->contract);
    s->contract = c ? c : goal_contract_new();
}
void goal_state_set_status(goal_state_t *s, goal_status_t st) {
    if (!s) return;
    free(s->status);
    s->status = strdup(status_to_str(st));
}
void goal_state_set_status_str(goal_state_t *s, const char *status) {
    if (!s) return;
    free(s->status);
    s->status = strdup(status && status[0] ? status : "active");
}
goal_status_t goal_state_status(const goal_state_t *s) {
    return s ? status_from_str(s->status) : GOAL_STATUS_ACTIVE;
}
const char *goal_state_status_str(const goal_state_t *s) {
    return s ? (s->status ? s->status : "active") : "active";
}
void goal_state_set_turns_used(goal_state_t *s, int n) { if (s) s->turns_used = n; }
int  goal_state_turns_used(const goal_state_t *s)    { return s ? s->turns_used : 0; }
int  goal_state_max_turns(const goal_state_t *s)     { return s ? s->max_turns : 0; }
void goal_state_set_max_turns(goal_state_t *s, int n) { if (s) s->max_turns = n; }
void goal_state_set_paused_reason(goal_state_t *s, const char *r) {
    if (!s) return;
    free(s->paused_reason);
    s->paused_reason = (r && r[0]) ? strdup(r) : NULL;
}
const char *goal_state_paused_reason(const goal_state_t *s) {
    return s ? (s->paused_reason ? s->paused_reason : "") : "";
}
const char *goal_state_goal(const goal_state_t *s) {
    return s ? (s->goal ? s->goal : "") : "";
}
void goal_state_set_last_verdict(goal_state_t *s, const char *v) {
    if (!s) return;
    free(s->last_verdict);
    s->last_verdict = v ? strdup(v) : NULL;
}
void goal_state_set_last_reason(goal_state_t *s, const char *r) {
    if (!s) return;
    free(s->last_reason);
    s->last_reason = r ? strdup(r) : NULL;
}
int goal_state_parse_failures(const goal_state_t *s) { return s ? s->consecutive_parse_failures : 0; }
void goal_state_set_parse_failures(goal_state_t *s, int n) { if (s) s->consecutive_parse_failures = n; }

/* ── OS liveness primitive ────────────────────────────────────────── */

/* PoP: goal_pid_alive @ hermes_cli/goals.py:_pid_alive */
bool goal_pid_alive(long pid) {
    if (pid <= 0) return false;
    /* kill(pid, 0) is the POSIX liveness probe: returns 0 if the process
     * exists and we have permission to signal it; ESRCH => dead, EPERM =>
     * exists but no permission (still alive). Any other error => treat dead
     * (fail-safe, mirroring Python's "any error resolves to False"). */
    if (kill((pid_t)pid, 0) == 0) return true;
    return (errno != ESRCH);
}

/* goal_session_waiting(): whether a goal parked on a session should stay
 * parked. Pure delegate: an injected callback reports liveness (wired by the
 * host that owns process_registry). Fail-safe: NULL callback => not waiting.
 * (Python: _session_waiting.) */
/* PoP: goal_session_waiting @ hermes_cli/goals.py:_session_waiting */
bool goal_session_waiting(int (*cb)(const char *session_id), const char *session_id) {
    if (!session_id || !session_id[0]) return false;
    if (!cb) return false;
    return cb(session_id) != 0;
}

char *goal_state_remove_subgoal(goal_state_t *s, int index_1based) {
    if (!s) return NULL;
    int idx = index_1based - 1;
    if (idx < 0 || (size_t)idx >= s->n_subgoals) return NULL;
    char *removed = s->subgoals[idx];
    s->subgoals[idx] = NULL;
    /* compact */
    size_t w = 0;
    for (size_t i = 0; i < s->n_subgoals; i++) {
        if (s->subgoals[i]) s->subgoals[w++] = s->subgoals[i];
    }
    s->n_subgoals = w;
    /* realloc down */
    if (s->n_subgoals == 0) { free(s->subgoals); s->subgoals = NULL; }
    else s->subgoals = realloc(s->subgoals, s->n_subgoals * sizeof(char *));
    return removed; /* caller owns */
}

/* ── GoalManager ──────────────────────────────────────────────────── */

struct goal_manager_t {
    char *session_id;
    int default_max_turns;
    const goal_manager_vtab_t *vtab;
    goal_state_t *state;   /* NULL when no goal loaded */
};

goal_manager_t *goal_manager_new(const char *session_id, const goal_manager_vtab_t *vtab, int default_max_turns) {
    goal_manager_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->session_id = strdup(session_id ? session_id : "");
    m->vtab = vtab;
    m->default_max_turns = (default_max_turns > 0) ? default_max_turns : GOAL_DEFAULT_MAX_TURNS;
    /* load existing goal via vtab */
    if (vtab && vtab->load) {
        char *raw = vtab->load(m->session_id);
        if (raw) {
            goal_state_t *s = goal_state_new("");
            if (goal_state_from_json(raw, s)) m->state = s;
            else goal_state_free(s);
            free(raw);
        }
    }
    return m;
}

void goal_manager_free(goal_manager_t *m) {
    if (!m) return;
    free(m->session_id);
    if (m->state) goal_state_free(m->state);
    free(m);
}

const char *goal_manager_session_id(const goal_manager_t *m) { return m ? m->session_id : ""; }
goal_state_t *goal_manager_state(goal_manager_t *m) { return m ? m->state : NULL; }

static void persist(goal_manager_t *m) {
    if (m && m->vtab && m->vtab->save && m->state) {
        char *js = goal_state_to_json(m->state);
        m->vtab->save(m->session_id, js);
        free(js);
    }
}

bool goal_manager_is_active(const goal_manager_t *m) {
    return m && m->state && strcmp(goal_state_status_str(m->state), "active") == 0;
}
bool goal_manager_has_goal(const goal_manager_t *m) {
    if (!m || !m->state) return false;
    const char *st = goal_state_status_str(m->state);
    return strcmp(st, "active") == 0 || strcmp(st, "paused") == 0;
}
bool goal_manager_has_contract(const goal_manager_t *m) {
    return m && m->state && goal_state_has_contract(m->state);
}

char *goal_manager_status_line(const goal_manager_t *m) {
    if (!m || !m->state) return strdup("No active goal. Set one with /goal <text>.");
    const goal_state_t *s = m->state;
    const char *st = goal_state_status_str(s);
    if (strcmp(st, "cleared") == 0)
        return strdup("No active goal. Set one with /goal <text>.");

    /* turns meta */
    char meta[256];
    char sub_buf[64] = "";
    if (s->n_subgoals)
        snprintf(sub_buf, sizeof(sub_buf), ", %zu subgoal%s",
                 s->n_subgoals, s->n_subgoals == 1 ? "" : "s");
    char con_buf[32] = "";
    if (goal_state_has_contract(s)) snprintf(con_buf, sizeof(con_buf), ", contract");
    snprintf(meta, sizeof(meta), "%d/%d turns%s%s",
             s->turns_used, s->max_turns, sub_buf, con_buf);

    char *out = NULL;
    if (strcmp(st, "active") == 0) {
        if (s->waiting_on_session && (!m->vtab || !m->vtab->session_waiting ||
                                       m->vtab->session_waiting(s->waiting_on_session))) {
            const char *wr = s->waiting_reason ? s->waiting_reason : s->waiting_on_session;
            size_t L = 64 + strlen(wr) + strlen(s->goal) + strlen(meta);
            out = malloc(L);
            snprintf(out, L, "\xe2\x8f\xb3 Goal (parked on %s, %s): %s", wr, meta, s->goal);
            return out;
        }
        if (s->waiting_on_pid && (!m->vtab || !m->vtab->pid_alive ||
/* PoP: pid_alive @ hermes_cli/active_sessions.py:_pid_alive */
/* PoP: pid_alive @ hermes_cli/kanban_db.py:_pid_alive */
                                  m->vtab->pid_alive(s->waiting_on_pid))) {
            char wr_buf[128];
            snprintf(wr_buf, sizeof(wr_buf), "pid %ld%s%s%s", s->waiting_on_pid,
                     s->waiting_reason ? " (" : "", s->waiting_reason ? s->waiting_reason : "",
                     s->waiting_reason ? ")" : "");
            size_t L = 64 + strlen(wr_buf) + strlen(s->goal) + strlen(meta);
            out = malloc(L);
            snprintf(out, L, "\xe2\x8f\xb3 Goal (parked on %s, %s): %s", wr_buf, meta, s->goal);
            return out;
        }
        if (s->waiting_until > 0) {
            double now = (double)time(NULL);
            if (now < s->waiting_until) {
                long remaining = (long)(s->waiting_until - now);
                const char *wr = s->waiting_reason ? s->waiting_reason : "";
                size_t L = 128 + strlen(wr) + strlen(s->goal) + strlen(meta);
                out = malloc(L);
                snprintf(out, L, "\xe2\x8f\xb3 Goal (parked %lds — %s, %s): %s",
                         remaining, wr, meta, s->goal);
                return out;
            }
        }
        size_t L = 64 + strlen(meta) + strlen(s->goal);
        out = malloc(L);
        snprintf(out, L, "\xe2\x8a\x99 Goal (active, %s): %s", meta, s->goal);
        return out;
    }
    if (strcmp(st, "paused") == 0) {
        const char *extra = s->paused_reason ? s->paused_reason : "";
        size_t L = 64 + strlen(meta) + strlen(extra) + strlen(s->goal);
        out = malloc(L);
        snprintf(out, L, "\xe2\x8f\xb8 Goal (paused, %s%s%s): %s",
                 meta, extra ? " — " : "", extra, s->goal);
        return out;
    }
    if (strcmp(st, "done") == 0) {
        size_t L = 64 + strlen(meta) + strlen(s->goal);
        out = malloc(L);
        snprintf(out, L, "\xe2\x9c\x93 Goal done (%s): %s", meta, s->goal);
        return out;
    }
    size_t L = 64 + strlen(st) + strlen(meta) + strlen(s->goal);
    out = malloc(L);
    snprintf(out, L, "Goal (%s, %s): %s", st, meta, s->goal);
    return out;
}

/* ── mutation ─────────────────────────────────────────────────────── */

goal_state_t *goal_manager_set(goal_manager_t *m, const char *goal, int max_turns, const goal_contract_t *contract) {
    if (!m) return NULL;
    char *g = goal ? strdup(goal) : strdup("");
    /* trim */
    char *p = g;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == '\0') { free(g); return NULL; } /* empty goal -> NULL (caller raises) */
    /* shift g to trimmed (in place) */
    memmove(g, p, strlen(p) + 1);

    if (m->state) goal_state_free(m->state);
    goal_state_t *s = goal_state_new(g);
    free(g);
    goal_state_set_status(s, GOAL_STATUS_ACTIVE);
    goal_state_set_turns_used(s, 0);
    goal_state_set_max_turns(s, max_turns > 0 ? max_turns : m->default_max_turns);
    goal_state_set_status(s, GOAL_STATUS_ACTIVE); /* created_at filled by new */
    s->created_at = (double)time(NULL);
    s->last_turn_at = 0.0;
    if (contract) {
        goal_contract_t *copy = goal_contract_new();
        for (int i = 0; i < 5; i++) {
            const char *fld = goal_contract_get(contract, CONTRACT_FIELD_NAME(i));
            goal_contract_set(copy, CONTRACT_FIELD_NAME(i), fld);
        }
        goal_state_set_contract(s, copy);
    }
    m->state = s;
    persist(m);
    return s;
}

goal_state_t *goal_manager_set_contract(goal_manager_t *m, const goal_contract_t *contract) {
    if (!m || !m->state) return NULL;
    goal_contract_t *copy = goal_contract_new();
    if (contract) {
        for (int i = 0; i < 5; i++) {
            const char *fld = goal_contract_get(contract, CONTRACT_FIELD_NAME(i));
            goal_contract_set(copy, CONTRACT_FIELD_NAME(i), fld);
        }
    }
    goal_state_set_contract(m->state, copy);
    persist(m);
    return m->state;
}

goal_state_t *goal_manager_pause(goal_manager_t *m, const char *reason) {
    if (!m || !m->state) return NULL;
    goal_state_set_status(m->state, GOAL_STATUS_PAUSED);
    goal_state_set_paused_reason(m->state, reason && reason[0] ? reason : "user-paused");
    m->state->waiting_on_pid = 0;
    if (m->state->waiting_on_session) { free(m->state->waiting_on_session); m->state->waiting_on_session = NULL; }
    m->state->waiting_until = 0.0;
    if (m->state->waiting_reason) { free(m->state->waiting_reason); m->state->waiting_reason = NULL; }
    m->state->waiting_since = 0.0;
    persist(m);
    return m->state;
}

goal_state_t *goal_manager_resume(goal_manager_t *m, bool reset_budget) {
    if (!m || !m->state) return NULL;
    goal_state_set_status(m->state, GOAL_STATUS_ACTIVE);
    goal_state_set_paused_reason(m->state, NULL);
    m->state->waiting_on_pid = 0;
    if (m->state->waiting_on_session) { free(m->state->waiting_on_session); m->state->waiting_on_session = NULL; }
    m->state->waiting_until = 0.0;
    if (m->state->waiting_reason) { free(m->state->waiting_reason); m->state->waiting_reason = NULL; }
    m->state->waiting_since = 0.0;
    if (reset_budget) goal_state_set_turns_used(m->state, 0);
    persist(m);
    return m->state;
}

goal_state_t *goal_manager_clear(goal_manager_t *m) {
    if (!m || !m->state) return NULL;
    goal_state_set_status(m->state, GOAL_STATUS_CLEARED);
    persist(m);
    goal_state_free(m->state);
    m->state = NULL;
    return NULL;
}

void goal_manager_mark_done(goal_manager_t *m, const char *reason) {
    if (!m || !m->state) return;
    goal_state_set_status(m->state, GOAL_STATUS_DONE);
    goal_state_set_last_verdict(m->state, "done");
    goal_state_set_last_reason(m->state, reason);
    persist(m);
}

char *goal_manager_add_subgoal(goal_manager_t *m, const char *text) {
    if (!m || !m->state || !goal_manager_has_goal(m)) return NULL;
    if (!text || !text[0]) return NULL;
    const char *p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t' || p[n-1] == '\n' || p[n-1] == '\r')) n--;
    if (n == 0) return NULL;
    char *clean = malloc(n + 1);
    memcpy(clean, p, n);
    clean[n] = '\0';
    if (!goal_state_add_subgoal(m->state, clean)) { free(clean); return NULL; }
    char *ret = strdup(clean);
    free(clean);
    persist(m);
    return ret; /* caller owns */
}

char *goal_manager_remove_subgoal(goal_manager_t *m, int index_1based) {
    if (!m || !m->state || !goal_manager_has_goal(m)) return NULL;
    char *removed = goal_state_remove_subgoal(m->state, index_1based);
    if (removed) persist(m);
    return removed; /* caller owns */
}

int goal_manager_clear_subgoals(goal_manager_t *m) {
    if (!m || !m->state || !goal_manager_has_goal(m)) return -1;
    int prev = (int)m->state->n_subgoals;
    for (size_t i = 0; i < m->state->n_subgoals; i++) free(m->state->subgoals[i]);
    free(m->state->subgoals);
    m->state->subgoals = NULL;
    m->state->n_subgoals = 0;
    persist(m);
    return prev;
}

char *goal_manager_render_subgoals(const goal_manager_t *m) {
    if (!m || !m->state) return strdup("(no active goal)");
    if (m->state->n_subgoals == 0) return strdup("(no subgoals — use /subgoal <text> to add criteria)");
    return goal_state_render_subgoals(m->state);
}

/* ── wait barriers ────────────────────────────────────────────────── */

goal_state_t *goal_manager_wait_on(goal_manager_t *m, long pid, const char *reason, double now) {
    if (!m || !m->state || strcmp(goal_state_status_str(m->state), "active") != 0) return NULL;
    if (pid <= 0) return NULL;
    m->state->waiting_on_pid = pid;
    if (m->state->waiting_on_session) { free(m->state->waiting_on_session); m->state->waiting_on_session = NULL; }
    m->state->waiting_until = 0.0;
    m->state->waiting_reason = (reason && reason[0]) ? strdup(reason) : NULL;
    m->state->waiting_since = now;
    persist(m);
    return m->state;
}

goal_state_t *goal_manager_wait_on_session(goal_manager_t *m, const char *session_id, const char *reason, double now) {
    if (!m || !m->state || strcmp(goal_state_status_str(m->state), "active") != 0) return NULL;
    if (!session_id || !session_id[0]) return NULL;
    if (m->state->waiting_on_session) free(m->state->waiting_on_session);
    m->state->waiting_on_session = strdup(session_id);
    m->state->waiting_on_pid = 0;
    m->state->waiting_until = 0.0;
    m->state->waiting_reason = (reason && reason[0]) ? strdup(reason) : NULL;
    m->state->waiting_since = now;
    persist(m);
    return m->state;
}

goal_state_t *goal_manager_wait_for_seconds(goal_manager_t *m, int seconds, const char *reason, double now) {
    if (!m || !m->state || strcmp(goal_state_status_str(m->state), "active") != 0) return NULL;
    if (seconds <= 0) return NULL;
    m->state->waiting_on_pid = 0;
    if (m->state->waiting_on_session) { free(m->state->waiting_on_session); m->state->waiting_on_session = NULL; }
    m->state->waiting_until = now + (double)seconds;
    m->state->waiting_reason = (reason && reason[0]) ? strdup(reason) : NULL;
    m->state->waiting_since = now;
    persist(m);
    return m->state;
}

bool goal_manager_stop_waiting(goal_manager_t *m) {
    if (!m || !m->state) return false;
    if (m->state->waiting_on_pid == 0 &&
        m->state->waiting_on_session == NULL &&
        m->state->waiting_until == 0.0) return false;
    m->state->waiting_on_pid = 0;
    if (m->state->waiting_on_session) { free(m->state->waiting_on_session); m->state->waiting_on_session = NULL; }
    m->state->waiting_until = 0.0;
    if (m->state->waiting_reason) { free(m->state->waiting_reason); m->state->waiting_reason = NULL; }
    m->state->waiting_since = 0.0;
    persist(m);
    return true;
}

bool goal_manager_is_waiting(goal_manager_t *m, double now) {
    if (!m || !m->state) return false;
    goal_state_t *s = m->state;
    if (s->waiting_on_session) {
        int alive = 0;
        if (m->vtab && m->vtab->session_waiting) alive = m->vtab->session_waiting(s->waiting_on_session);
        if (alive) return true;
        goal_manager_stop_waiting(m);
        return false;
    }
    if (s->waiting_on_pid != 0) {
        int alive = (m->vtab && m->vtab->pid_alive) ? m->vtab->pid_alive(s->waiting_on_pid)
                                                    : goal_pid_alive(s->waiting_on_pid);
        if (alive) return true;
        goal_manager_stop_waiting(m);
        return false;
    }
    if (s->waiting_until > 0.0) {
        if (now < s->waiting_until) return true;
        goal_manager_stop_waiting(m);
        return false;
    }
    return false;
}

/* ── continuation prompt + contract render ───────────────────────── */

char *goal_manager_next_continuation_prompt(const goal_manager_t *m) {
    if (!m || !m->state || strcmp(goal_state_status_str(m->state), "active") != 0) return NULL;
    const goal_state_t *s = m->state;
    if (goal_state_has_contract(s)) {
        char *block = goal_contract_render(s->contract);
        char *final_block = block;
        if (s->n_subgoals) {
            /* append "Extra criterion N:" lines */
            size_t cap = strlen(block) + s->n_subgoals * 256 + 16;
            char *combined = malloc(cap);
            snprintf(combined, cap, "%s\n", block);
            char *p = combined + strlen(combined);
            for (size_t i = 0; i < s->n_subgoals; i++) {
                int n = snprintf(p, cap - (size_t)(p - combined), "- Extra criterion %zu: %s\n", i + 1, s->subgoals[i]);
                p += n;
            }
            final_block = combined;
        }
        const char *goal = s->goal;
        size_t L = 1024 + strlen(goal) + strlen(final_block);
        char *out = malloc(L);
        snprintf(out, L,
            "[Continuing toward your standing goal]\n"
            "Goal: %s\n\n"
            "Completion contract:\n"
            "%s\n\n"
            "Continue working toward the outcome above. Take the next concrete step. "
            "Stay within the stated boundaries and do not violate the constraints. "
            "Before claiming the goal is done, satisfy the Verification criterion and "
            "show the concrete evidence (command output, file contents, test result). "
            "If you hit the stated stop condition or are otherwise blocked and need "
            "user input, say so clearly and stop.",
            goal, final_block);
        free(final_block);
        return out;
    }
    if (s->n_subgoals) {
        char *sub = goal_state_render_subgoals(s);
        const char *goal = s->goal;
        size_t L = 1024 + strlen(goal) + strlen(sub);
        char *out = malloc(L);
        snprintf(out, L,
            "[Continuing toward your standing goal]\n"
            "Goal: %s\n\n"
            "Additional criteria the user added mid-loop:\n"
            "%s\n\n"
            "Continue working toward the goal AND all additional criteria. Take "
            "the next concrete step. If you believe the goal and every "
            "additional criterion are complete, state so explicitly and stop. "
            "If you are blocked and need input from the user, say so clearly "
            "and stop.",
            goal, sub);
        free(sub);
        return out;
    }
    const char *goal = s->goal;
    size_t L = 512 + strlen(goal);
    char *out = malloc(L);
    snprintf(out, L,
        "[Continuing toward your standing goal]\n"
        "Goal: %s\n\n"
        "Continue working toward this goal. Take the next concrete step. "
        "If you believe the goal is complete, state so explicitly and stop. "
        "If you are blocked and need input from the user, say so clearly and stop.",
        goal);
    return out;
}

char *goal_manager_render_contract(const goal_manager_t *m) {
    if (!m || !m->state) return strdup("(no active goal)");
    if (!goal_state_has_contract(m->state))
        return strdup("(no completion contract — set one with /goal draft <objective> or inline field: value lines)");
    return goal_contract_render(m->state->contract);
}

/* ── judge prompt helpers ─────────────────────────────────────────── */

int goal_judge_max_tokens_resolved(int (*resolve)(void)) {
    if (resolve) {
        int v = resolve();
        if (v > 0) return v;
    }
    return 4096; /* DEFAULT_JUDGE_MAX_TOKENS */
}

static char *extract_json(const char *text) {
    if (!text) return NULL;
    char *dup = strdup(text);
    /* strip ``` fences */
    char *p = dup;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (p[0] == '`' && p[1] == '`') {
        /* find first newline after opening fence */
        char *nl = strchr(p, '\n');
        if (nl) p = nl + 1;
    }
    char *json = NULL;
    char *err = NULL;
    json_t *doc = json_parse(p, &err);
    if (err) { free(err); }
    else if (doc) {
        char *tmp = json_serialize(doc);
        json_free(doc);
        json = tmp;
    }
    if (!json) {
        /* regex-like first {...} */
        const char *start = strchr(p, '{');
        if (start) {
            int depth = 0;
            const char *q = start;
            for (; *q; q++) {
                if (*q == '{') depth++;
                else if (*q == '}') { depth--; if (depth == 0) { q++; break; } }
            }
            if (depth == 0 && q > start) {
                size_t len = (size_t)(q - start);
                json = malloc(len + 1);
                memcpy(json, start, len);
                json[len] = '\0';
            }
        }
    }
    free(dup);
    return json;
}

char *goal_judge_extract_json_object(const char *raw) {
    if (!raw || !raw[0]) return NULL;
    char *text = strdup(raw);
    /* strip leading/trailing ws + fences */
    char *p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t' || p[n-1] == '\n' || p[n-1] == '\r')) n--;
    p[n] = '\0';
    if (p[0] == '`' && p[1] == '`') {
        char *nl = strchr(p, '\n');
        if (nl) { memmove(p, nl + 1, strlen(nl + 1) + 1); }
    }
    char *json = extract_json(p);
    free(text);
    return json;
}

void goal_judge_parse_response(const char *raw,
                               char **verdict_out, char **reason_out,
                               bool *parse_failed_out, char **wait_directive_out) {
    char *verdict = strdup("continue");
    char *reason = strdup("");
    bool parse_failed = false;
    char *wait = NULL;

    char *json = goal_judge_extract_json_object(raw);
    if (!json) {
        if (!raw || !raw[0]) {
            free(verdict); verdict = strdup("continue");
            free(reason); reason = strdup("judge returned empty response");
        } else {
            free(reason); reason = malloc(strlen(raw) + 64);
            snprintf(reason, strlen(raw) + 64, "judge reply was not JSON: %s", raw);
        }
        parse_failed = true;
        *verdict_out = verdict; *reason_out = reason; *parse_failed_out = parse_failed; *wait_directive_out = NULL;
        return;
    }

    char *err = NULL;
    json_t *doc = json_parse(json, &err);
    free(json);
    if (err) { free(err); }
    if (!doc || doc->type != JSON_OBJECT) {
        free(reason); reason = strdup("judge reply was not a JSON object");
        parse_failed = true;
        free(doc);
        *verdict_out = verdict; *reason_out = reason; *parse_failed_out = parse_failed; *wait_directive_out = NULL;
        return;
    }

    const char *r = json_get_str(doc, "reason", NULL);
    free(reason);
    reason = strdup(r && r[0] ? r : "no reason provided");

    /* verdict */
    const char *vr = json_get_str(doc, "verdict", NULL);
    if (vr) {
        free(verdict);
        verdict = strdup(vr);
        /* lowercase */
        for (char *c = verdict; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
    } else {
        json_t *done_v = json_obj_get(doc, "done");
        bool done = false;
        if (done_v) {
            if (done_v->type == JSON_BOOL) done = done_v->bool_val;
            else if (done_v->type == JSON_STRING) {
                const char *ds = done_v->str_val;
                done = (strcmp(ds, "true") == 0 || strcmp(ds, "yes") == 0 ||
                        strcmp(ds, "1") == 0 || strcmp(ds, "done") == 0);
            }
        }
        free(verdict);
        verdict = strdup(done ? "done" : "continue");
    }
    if (strcmp(verdict, "done") != 0 && strcmp(verdict, "continue") != 0 && strcmp(verdict, "wait") != 0) {
        free(verdict);
        verdict = strdup("continue");
    }

    if (strcmp(verdict, "wait") == 0) {
        /* extract directive */
        const char *sess = json_get_str(doc, "wait_on_session", NULL);
        if (!sess) sess = json_get_str(doc, "session_id", NULL);
        if (!sess) sess = json_get_str(doc, "wait_session", NULL);
        if (sess && sess[0]) {
            size_t L = strlen(sess) + 32;
            wait = malloc(L);
            snprintf(wait, L, "{\"session_id\":\"%s\"}", sess);
        } else {
            long pid = 0;
            json_t *pv = json_obj_get(doc, "wait_on_pid");
            if (!pv) pv = json_obj_get(doc, "pid");
            if (!pv) pv = json_obj_get(doc, "wait_pid");
            if (pv && pv->type == JSON_NUMBER) pid = (long)pv->num_val;
            if (pid <= 0 && pv && pv->type == JSON_STRING) pid = atol(pv->str_val);
            if (pid > 0) {
                size_t L = 32;
                wait = malloc(L);
                snprintf(wait, L, "{\"pid\":%ld}", pid);
            } else {
                long sec = 0;
                json_t *sv = json_obj_get(doc, "wait_for_seconds");
                if (!sv) sv = json_obj_get(doc, "seconds");
                if (!sv) sv = json_obj_get(doc, "wait_seconds");
                if (sv && sv->type == JSON_NUMBER) sec = (long)sv->num_val;
                if (sec <= 0 && sv && sv->type == JSON_STRING) sec = atol(sv->str_val);
                if (sec > 0) {
                    size_t L = 32;
                    wait = malloc(L);
                    snprintf(wait, L, "{\"seconds\":%ld}", sec);
                }
            }
        }
        if (!wait) {
            /* downgrade to continue */
            free(verdict); verdict = strdup("continue");
            char *old_reason = reason;
            size_t L = strlen(old_reason) + 64;
            reason = malloc(L);
            snprintf(reason, L, "%s (wait verdict had no target — continuing)", old_reason);
            free(old_reason);
        }
    }

    json_free(doc);
    *verdict_out = verdict;
    *reason_out = reason;
    *parse_failed_out = parse_failed;
    *wait_directive_out = wait;
}

/* render background block from an array of JSON process strings */
char *goal_judge_render_background_block(const char *const *json_processes) {
    if (!json_processes) return strdup("");
    size_t cap = 1024, len = 0;
    char *lines = malloc(cap);
    if (!lines) return strdup("");
    lines[0] = '\0';

    for (size_t i = 0; json_processes[i]; i++) {
        char *err = NULL;
        json_t *p = json_parse(json_processes[i], &err);
        if (err) { free(err); continue; }
        if (!p || p->type != JSON_OBJECT) { json_free(p); continue; }
        const char *status = json_get_str(p, "status", "");
        if (status && strcmp(status, "exited") == 0) { json_free(p); continue; }
        long pid = 0;
        json_t *pv = json_obj_get(p, "pid");
        if (pv && pv->type == JSON_NUMBER) pid = (long)pv->num_val;
        else if (pv && pv->type == JSON_STRING) pid = atol(pv->str_val);
        if (pid == 0) { json_free(p); continue; }

        const char *cmd = json_get_str(p, "command", "");
        const char *sid = json_get_str(p, "session_id", NULL);
        /* truncate cmd to 120 */
        char cmd_t[256];
        truncate_copy(cmd, cmd_t, 120);
        const char *op = json_get_str(p, "output_preview", "");
        char op_t[256];
        truncate_copy(op, op_t, 120);

        /* build line */
        char line[1024];
        int L = snprintf(line, sizeof(line), "- pid %ld", pid);
        if (sid && sid[0]) L += snprintf(line + L, sizeof(line) - L, " / session %s", sid);
        L += snprintf(line + L, sizeof(line) - L, ": %s", cmd_t);
        json_t *up = json_obj_get(p, "uptime_seconds");
        if (up && up->type == JSON_NUMBER) L += snprintf(line + L, sizeof(line) - L, " (running %lds)", (long)up->num_val);
        json_t *wps = json_obj_get(p, "watch_patterns");
        if (wps) {
            const char *wp = json_get_str(p, "watch_hit", NULL);
            char *wps_s = json_serialize(wps);
            L += snprintf(line + L, sizeof(line) - L, " | watch_patterns=%s%s", wps_s ? wps_s : "", wp ? " [already matched]" : "");
            free(wps_s);
        } else {
            json_t *noc = json_obj_get(p, "notify_on_complete");
            if (noc && ((noc->type == JSON_BOOL && noc->bool_val) || (noc->type == JSON_STRING && noc->str_val && noc->str_val[0])))
                L += snprintf(line + L, sizeof(line) - L, " | notify_on_complete");
        }
        if (op_t[0]) L += snprintf(line + L, sizeof(line) - L, " | recent output: %s", op_t);
        L += snprintf(line + L, sizeof(line) - L, "\n");

        if (len + L + 1 > cap) { cap = (len + L + 1) * 2; lines = realloc(lines, cap); }
        memcpy(lines + len, line, L);
        len += L;
        lines[len] = '\0';
        json_free(p);
    }
    if (len == 0) { free(lines); return strdup(""); }
    /* wrap in template */
    size_t total = len + 128;
    char *out = malloc(total);
    snprintf(out, total,
        "Background processes the agent currently has running (it may be waiting "
        "on one of these):\n%s\n", lines);
    free(lines);
    return out;
}

/* PoP: gather_background_processes @ hermes_cli/goals.py:gather_background_processes */
/*
 * Faithful C port of goals.py:gather_background_processes.
 *
 * Thin, fail-safe wrapper over process_registry_list(task_id). Returns only
 * RUNNING processes (an exited one is nothing to wait on) as a JSON array
 * string the caller frees. Never raises: any registry failure yields "[]".
 */
char *goal_gather_background_processes(const char *task_id) {
    char *raw = process_registry_list(task_id);
    if (!raw) return strdup("[]");

    json_t *doc = json_parse(raw, NULL);
    free(raw);
    if (!doc || doc->type != JSON_ARRAY) {
        json_free(doc);
        return strdup("[]");
    }

    /* Build a filtered array: drop entries with status == "exited". */
    size_t cap = 4096;
    char *out = malloc(cap);
    size_t len = 0;
    out[len++] = '[';

    int first = 1;
    for (size_t i = 0; i < doc->c.count; i++) {
        json_t *e = doc->c.items[i];
        if (!e || e->type != JSON_OBJECT) continue;
        const char *status = json_get_str(e, "status", NULL);
        if (status && strcmp(status, "exited") == 0) continue;

        /* Re-serialize the entry. Each list entry is a compact JSON object. */
        char *entry = json_serialize(e);
        if (!entry) continue;
        size_t need = len + strlen(entry) + 2; /* comma + entry + nul */
        if (need > cap) { cap = need * 2; out = realloc(out, cap); }
        if (!first) out[len++] = ',';
        memcpy(out + len, entry, strlen(entry));
        len += strlen(entry);
        first = 0;
        free(entry);
    }
    out[len++] = ']';
    out[len] = '\0';
    json_free(doc);
    return out;
}
