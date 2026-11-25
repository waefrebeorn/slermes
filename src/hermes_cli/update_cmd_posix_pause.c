/*
 * update_cmd_posix_pause.c - C11 port of upstream
 * hermes_cli/update_cmd_posix_pause.py (gates w30_updatecmdp_fa52e9_p1 and
 * w30_updatecmdp_fa52e9_p2).
 *
 * PoP: w30_updatecmdp_fa52e9_p1_empty_token        @ hermes_cli/update_cmd_posix_pause.py:_empty_token
 * PoP: w30_updatecmdp_fa52e9_p1_gateways_paused    @ hermes_cli/update_cmd_posix_pause.py:gateways_paused
 * PoP: w30_updatecmdp_fa52e9_p1_inside             @ hermes_cli/update_cmd_posix_pause.py:_inside
 * PoP: w30_updatecmdp_fa52e9_p1_scope_cmd          @ hermes_cli/update_cmd_posix_pause.py:_scope_cmd
 * PoP: w30_updatecmdp_fa52e9_p1_manage_cmd         @ hermes_cli/update_cmd_posix_pause.py:_manage_cmd
 * PoP: w30_updatecmdp_fa52e9_p1_unit_stop_pending  @ hermes_cli/update_cmd_posix_pause.py:unit_stop_pending
 * PoP: w30_updatecmdp_fa52e9_p1_alive              @ hermes_cli/update_cmd_posix_pause.py:_alive
 * PoP: w30_updatecmdp_fa52e9_p1_is_left_pending    @ hermes_cli/update_cmd_posix_pause.py:is_left_pending
 * PoP: w30_updatecmdp_fa52e9_p1_adopt_left_pending @ hermes_cli/update_cmd_posix_pause.py:adopt_left_pending
 * PoP: w30_updatecmdp_fa52e9_p2_names              @ hermes_cli/update_cmd_posix_pause.py:_names
 * PoP: w30_updatecmdp_fa52e9_p2_describe           @ hermes_cli/update_cmd_posix_pause.py:_describe
 * PoP: w30_updatecmdp_fa52e9_p2_drain_ack          @ hermes_cli/update_cmd_posix_pause.py:_ask_to_drain
 * PoP: w30_updatecmdp_fa52e9_p2_supervisor_marker  @ hermes_cli/update_cmd_posix_pause.py:_supervisor_markers
 * PoP: w30_updatecmdp_fa52e9_p2_left_running       @ hermes_cli/update_cmd_posix_pause.py:_left_running
 * PoP: w30_updatecmdp_fa52e9_p2_abort_unsettled    @ hermes_cli/update_cmd_posix_pause.py:_abort_unsettled
 * PoP: w30_updatecmdp_fa52e9_p2_bare_replay_env    @ hermes_cli/update_cmd_posix_pause.py:_bare_replay_env
 * PoP: w30_updatecmdp_fa52e9_p2_gateway_on_home    @ hermes_cli/update_cmd_posix_pause.py:_gateway_on_home
 *
 * WHAT IS A SEAM (fed as fixture input, never executed here): systemctl and
 * busctl, /proc/<pid>/cgroup|stat|cmdline, psutil.Process().environ(),
 * launchctl, the control socket (pause_gateway_for_update), the launchd and
 * systemd discovery listings, gateway.status liveness + start times, the pause
 * record (record_pause/mark_stop_sent/sync) and every subprocess.
 *
 * WHAT IS PORTED: the part that is a pure function of those values - the pause
 * token's shape and predicate, cgroup containment, the systemctl scope and
 * sudo prefixes, the "stop has settled" predicate, the start-time identity
 * rules (alive / left pending / adopted), the naming and description of a
 * paused set, the drain-ACK predicate, the supervisor attribution of a bare
 * gateway, which runtimes must be left running, the --force refusal text and
 * its token mutations, the replay environment cleanup and the adopted-pid rule.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w30_updatecmdp_fa52e9_p1.h"
#include "noc_w30_updatecmdp_fa52e9_p2.h"

/* ------------------------------------------------------------------ values */

/* Python truthiness of a JSON value. */
static int pp_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python's numeric view: an int, a float or a bool. */
static int pp_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* The string value of a JSON string, else NULL. */
static const char *pp_text(const json_t *v) {
    return v && v->type == JSON_STRING ? (v->str_val ? v->str_val : "") : NULL;
}

/* Python int(value): tolerant of numeric strings, like the upstream int(pid). */
static int pp_to_int(const json_t *v, long long *out) {
    double d;
    const char *s;
    char *end = NULL;
    long long n;
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (pp_number(v, &d)) { *out = (long long)d; return 1; }
    s = pp_text(v);
    if (!s || !*s) return 0;
    n = strtoll(s, &end, 10);
    if (end == s) return 0;
    while (end && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end++;
    if (end && *end != '\0') return 0;
    *out = n;
    return 1;
}

/* Python repr of an int for f-strings ("gateway PID 4242"). */
static void pp_int_text(char *buf, size_t cap, double v) {
    if (v == (double)(long long)v) snprintf(buf, cap, "%.0f", v);
    else snprintf(buf, cap, "%.17g", v);
}

/* Python str() of the shapes an environment variable can hold. */
static void pp_str_text(char *buf, size_t cap, const json_t *v) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    if (v->type == JSON_BOOL) { snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) { pp_int_text(buf, cap, v->num_val); return; }
    if (v->type == JSON_STRING) { snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return; }
    snprintf(buf, cap, "%s", v->type == JSON_ARRAY ? "[]" : "{}");
}

/* ---------------------------------------------------------------- builder */

typedef struct {
    char  *p;
    size_t len;
    size_t cap;
} pp_sb;

static void sb_reserve(pp_sb *b, size_t extra) {
    size_t need = b->len + extra + 1;
    char *np;
    if (b->cap && need <= b->cap) return;
    if (b->cap == 0) b->cap = 128;
    while (b->cap < need) b->cap *= 2;
    np = (char *)realloc(b->p, b->cap);
    if (!np) { free(b->p); b->p = NULL; b->cap = b->len = 0; return; }
    b->p = np;
}

static void sb_puts(pp_sb *b, const char *s) {
    size_t n;
    if (!b->cap) sb_reserve(b, 1);
    if (!b->p || !s) return;
    n = strlen(s);
    sb_reserve(b, n);
    if (!b->p) return;
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void sb_printf(pp_sb *b, const char *fmt, ...) {
    va_list ap;
    int n;
    if (!b->cap) sb_reserve(b, 1);
    if (!b->p) return;
    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    sb_reserve(b, (size_t)n);
    if (!b->p) return;
    va_start(ap, fmt);
    vsnprintf(b->p + b->len, b->cap - b->len, fmt, ap);
    va_end(ap);
    b->len += (size_t)n;
}

static char *sb_take(pp_sb *b) {
    char *p = b->p;
    if (!p) {
        p = (char *)malloc(1);
        if (p) p[0] = '\0';
    }
    b->p = NULL;
    b->len = b->cap = 0;
    return p;
}

/* ================================================================== gate p1 */

/* _empty_token(): the token this run arms before anything is stopped. */
json_t *w30_updatecmdp_fa52e9_p1_empty_token(void) {
    json_t *o = json_object();
    if (!o) return json_null();
    json_set(o, "platform", json_string("posix"));
    json_set(o, "resume_needed", json_bool(false));
    json_set(o, "profiles", json_object());
    json_set(o, "unmapped_pids", json_array());
    json_set(o, "unmapped", json_array());
    json_set(o, "posix_units", json_array());
    return o;
}

/* gateways_paused(): bool(_RUN and _RUN.get("posix_stopped") and _RUN.get("resume_needed")) */
json_t *w30_updatecmdp_fa52e9_p1_gateways_paused(const json_t *req) {
    const json_t *tok = json_obj_get(req, "token");
    int paused = 0;
    if (tok && tok->type == JSON_OBJECT)
        paused = pp_truthy(json_obj_get(tok, "posix_stopped"))
              && pp_truthy(json_obj_get(tok, "resume_needed"));
    return json_bool(paused);
}

/* _inside(): a cgroup path is inside a unit's when equal or under it + "/". */
static int pp_inside(const char *cgroup, const char *unit_cgroup) {
    size_t n;
    if (!cgroup || !cgroup[0] || !unit_cgroup || !unit_cgroup[0]) return 0;
    if (strcmp(cgroup, unit_cgroup) == 0) return 1;
    n = strlen(unit_cgroup);
    while (n > 0 && unit_cgroup[n - 1] == '/') n--;   /* unit_cgroup.rstrip("/") */
    if (strncmp(cgroup, unit_cgroup, n) != 0) return 0;
    return cgroup[n] == '/';
}

json_t *w30_updatecmdp_fa52e9_p1_inside(const json_t *req) {
    return json_bool(pp_inside(pp_text(json_obj_get(req, "cgroup")),
                               pp_text(json_obj_get(req, "unit_cgroup"))));
}

/* _scope_cmd(): the read-only systemctl prefix of a unit's scope. */
json_t *w30_updatecmdp_fa52e9_p1_scope_cmd(const json_t *req) {
    json_t *arr = json_array();
    const char *scope = pp_text(json_obj_get(req, "scope"));
    if (!arr) return json_null();
    json_append(arr, json_string("systemctl"));
    if (scope && strcmp(scope, "user") == 0) json_append(arr, json_string("--user"));
    return arr;
}

/* _manage_cmd(): the manage-units prefix, None when a system unit's stop needs
 * an interactive sudo. */
json_t *w30_updatecmdp_fa52e9_p1_manage_cmd(const json_t *req) {
    const char *scope = pp_text(json_obj_get(req, "scope"));
    int is_system = scope && strcmp(scope, "system") == 0;
    int is_user = scope && strcmp(scope, "user") == 0;
    int needs_sudo = pp_truthy(json_obj_get(req, "needs_sudo"));
    int sudo_ok = pp_truthy(json_obj_get(req, "sudo_ok"));
    json_t *arr;

    if (is_system && needs_sudo && !sudo_ok) return json_null();
    arr = json_array();
    if (!arr) return json_null();
    if (is_system && needs_sudo) {
        json_append(arr, json_string("sudo"));
        json_append(arr, json_string("-n"));
    }
    json_append(arr, json_string("systemctl"));
    if (is_user) json_append(arr, json_string("--user"));
    json_append(arr, json_string("--no-ask-password"));
    return arr;
}

/* _settled(): the unit is past every transitional state and has no queued job. */
static int pp_settled(const json_t *state) {
    static const char *const transitional[] = {
        "activating", "deactivating", "reloading", "refreshing",
    };
    const json_t *act = state ? json_obj_get(state, "ActiveState") : NULL;
    const char *text = pp_text(act);
    size_t i;

    if (!act || act->type == JSON_NULL) return 0;
    if (text) {
        if (!text[0]) return 0;
        for (i = 0; i < sizeof(transitional) / sizeof(transitional[0]); i++)
            if (strcmp(text, transitional[i]) == 0) return 0;
    }
    return !pp_truthy(state ? json_obj_get(state, "Job") : NULL);
}

/* unit_stop_pending(): a systemd unit whose stop has not settled.  Unreadable
 * counts as pending; a launchd job is never held here. */
json_t *w30_updatecmdp_fa52e9_p1_unit_stop_pending(const json_t *req) {
    const char *kind = pp_text(json_obj_get(req, "kind"));
    const json_t *state = json_obj_get(req, "state");
    if (!kind || strcmp(kind, "systemd") != 0) return json_bool(false);
    if (!state || state->type != JSON_OBJECT) state = NULL;   /* unreadable -> {} */
    return json_bool(!pp_settled(state));
}

/* _alive(): unknown liveness counts as alive (never relaunch a twin); a start
 * time unknown on either side counts as the same process. */
json_t *w30_updatecmdp_fa52e9_p1_alive(const json_t *req) {
    const char *exists = pp_text(json_obj_get(req, "pid_exists"));
    const json_t *ct = json_obj_get(req, "ct");
    const json_t *start = json_obj_get(req, "start_time");
    double a = 0.0, b = 0.0;

    if (exists && strcmp(exists, "no") == 0) return json_bool(false);
    if (exists && strcmp(exists, "error") == 0) return json_bool(true);
    if (!ct || ct->type == JSON_NULL) return json_bool(true);
    if (!start || start->type == JSON_NULL) return json_bool(true);
    if (!pp_number(ct, &a) || !pp_number(start, &b)) return json_bool(true);
    return json_bool(a == b);
}

/* is_left_pending(): the very process the pause left running, not a recycled PID. */
json_t *w30_updatecmdp_fa52e9_p1_is_left_pending(const json_t *req) {
    const json_t *pairs = json_obj_get(req, "left_pending");
    const json_t *cur = json_obj_get(req, "ct");
    const json_t *born = NULL;
    long long want = 0, pid = 0;
    double b = 0.0, c = 0.0;
    size_t i, n;

    if (!pp_to_int(json_obj_get(req, "pid"), &want)) return json_bool(false);
    if (pairs && pairs->type == JSON_ARRAY) {
        n = json_len(pairs);
        for (i = 0; i < n; i++) {
            const json_t *pair = json_get(pairs, i);
            if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
            if (!pp_to_int(json_get(pair, 0), &pid)) continue;
            if (pid == want) born = json_get(pair, 1);   /* _LEFT_PENDING[int(pid)] = ct */
        }
    }
    if (!born || born->type == JSON_NULL) return json_bool(false);   /* born is not None */
    if (!cur || !pp_number(cur, &c)) return json_bool(false);
    if (!pp_number(born, &b)) return json_bool(false);
    return json_bool(b == c);
}

typedef struct {
    long long pid;
    const json_t *ct;    /* borrowed from the request */
} pp_pair;

static int pp_pair_cmp(const void *a, const void *b) {
    long long x = ((const pp_pair *)a)->pid;
    long long y = ((const pp_pair *)b)->pid;
    return x < y ? -1 : (x > y ? 1 : 0);
}

/* adopt_left_pending(): restore the pairs whose PID still carries its start
 * time; a gone or recycled PID is dropped. */
json_t *w30_updatecmdp_fa52e9_p1_adopt_left_pending(const json_t *req) {
    const json_t *pairs = json_obj_get(req, "pairs");
    const json_t *starts = json_obj_get(req, "start_times");
    pp_pair *kept;
    json_t *out;
    size_t i, n, nk = 0;

    out = json_array();
    if (!out) return json_null();
    if (!pairs || pairs->type != JSON_ARRAY) return out;
    n = json_len(pairs);
    if (n == 0) return out;
    kept = (pp_pair *)calloc(n, sizeof(pp_pair));
    if (!kept) return out;

    for (i = 0; i < n; i++) {
        const json_t *pair = json_get(pairs, i);
        const json_t *ct, *live;
        long long pid = 0;
        char key[32];
        double want = 0.0, got = 0.0;
        size_t j;

        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
        ct = json_get(pair, 1);
        if (!ct || ct->type == JSON_NULL) continue;          /* ct is not None */
        if (!pp_to_int(json_get(pair, 0), &pid)) continue;
        if (!pp_number(ct, &want)) continue;
        snprintf(key, sizeof(key), "%lld", pid);
        live = starts && starts->type == JSON_OBJECT ? json_obj_get(starts, key) : NULL;
        if (!live || !pp_number(live, &got) || got != want) continue;
        for (j = 0; j < nk; j++) if (kept[j].pid == pid) break;
        if (j < nk) { kept[j].ct = ct; continue; }            /* dict overwrite */
        kept[nk].pid = pid;
        kept[nk].ct = ct;
        nk++;
    }
    qsort(kept, nk, sizeof(pp_pair), pp_pair_cmp);
    for (i = 0; i < nk; i++) {
        json_t *row = json_array();
        if (!row) continue;
        json_append(row, json_number((double)kept[i].pid));
        json_append(row, json_copy(kept[i].ct));
        json_append(out, row);
    }
    free(kept);
    return out;
}

/* ================================================================== gate p2 */

/* _names(): ", ".join(e.get("unit") or e.get("label") or f"gateway PID {e['pid']}") */
json_t *w30_updatecmdp_fa52e9_p2_names(const json_t *req) {
    const json_t *entries = json_obj_get(req, "entries");
    pp_sb b = {NULL, 0, 0};
    size_t i, n;
    char *text;
    json_t *out;

    if (entries && entries->type == JSON_ARRAY) {
        n = json_len(entries);
        for (i = 0; i < n; i++) {
            const json_t *e = json_get(entries, i);
            const char *unit, *label;
            double d = 0.0;
            char num[64];
            if (!e || e->type != JSON_OBJECT) continue;
            if (b.len) sb_puts(&b, ", ");
            unit = pp_text(json_obj_get(e, "unit"));
            label = pp_text(json_obj_get(e, "label"));
            if (pp_truthy(json_obj_get(e, "unit"))) {
                sb_puts(&b, unit ? unit : "");
            } else if (pp_truthy(json_obj_get(e, "label"))) {
                sb_puts(&b, label ? label : "");
            } else {
                if (pp_number(json_obj_get(e, "pid"), &d)) pp_int_text(num, sizeof(num), d);
                else snprintf(num, sizeof(num), "%s",
                              pp_text(json_obj_get(e, "pid")) ? pp_text(json_obj_get(e, "pid")) : "None");
                sb_printf(&b, "gateway PID %s", num);
            }
        }
    }
    text = sb_take(&b);
    out = json_string(text ? text : "");
    free(text);
    return out;
}

/* _describe(): posix_units by unit/label, then the bare gateways replayable
 * from their argv. */
json_t *w30_updatecmdp_fa52e9_p2_describe(const json_t *req) {
    const json_t *token = json_obj_get(req, "token");
    const json_t *units = token && token->type == JSON_OBJECT ? json_obj_get(token, "posix_units") : NULL;
    const json_t *unmapped = token && token->type == JSON_OBJECT ? json_obj_get(token, "unmapped") : NULL;
    pp_sb b = {NULL, 0, 0};
    size_t i, n;
    char *text;
    json_t *out;

    if (units && units->type == JSON_ARRAY) {
        n = json_len(units);
        for (i = 0; i < n; i++) {
            const json_t *u = json_get(units, i);
            const char *unit, *label;
            if (!u || u->type != JSON_OBJECT) continue;
            unit = pp_text(json_obj_get(u, "unit"));
            label = pp_text(json_obj_get(u, "label"));
            if (b.len) sb_puts(&b, ", ");
            if (pp_truthy(json_obj_get(u, "unit"))) sb_puts(&b, unit ? unit : "");
            else sb_puts(&b, label ? label : "");
        }
    }
    if (unmapped && unmapped->type == JSON_ARRAY) {
        n = json_len(unmapped);
        for (i = 0; i < n; i++) {
            const json_t *u = json_get(unmapped, i);
            const json_t *pid;
            double d = 0.0;
            char num[64];
            if (!u || u->type != JSON_OBJECT) continue;
            if (!pp_truthy(json_obj_get(u, "argv"))) continue;
            pid = json_obj_get(u, "pid");
            if (pp_number(pid, &d)) pp_int_text(num, sizeof(num), d);
            else snprintf(num, sizeof(num), "%s", pp_text(pid) ? pp_text(pid) : "None");
            if (b.len) sb_puts(&b, ", ");
            sb_printf(&b, "gateway PID %s", num);
        }
    }
    text = sb_take(&b);
    out = json_string(text ? text : "");
    free(text);
    return out;
}

/* _ask_to_drain(): True only on a control-socket ACK that took the drain. */
json_t *w30_updatecmdp_fa52e9_p2_drain_ack(const json_t *req) {
    const json_t *ack = json_obj_get(req, "ack");

    if (!pp_truthy(json_obj_get(req, "home"))) return json_bool(false);
    if (pp_truthy(json_obj_get(req, "raises"))) return json_bool(false);
    if (!ack || ack->type != JSON_OBJECT) return json_bool(false);
    return json_bool(pp_truthy(json_obj_get(ack, "pausing"))
                     || pp_truthy(json_obj_get(ack, "already_stopping")));
}

/* _supervisor_markers(): why *pid* answers to a supervisor other than a hermes
 * unit/job, or None. */
json_t *w30_updatecmdp_fa52e9_p2_supervisor_marker(const json_t *req) {
    const json_t *env = json_obj_get(req, "env");
    const json_t *unit = json_obj_get(req, "respawning_unit");
    const char *unit_text = pp_text(unit);
    const json_t *xpc;
    char text[512];
    pp_sb b = {NULL, 0, 0};
    char *out_text;
    json_t *out;

    if (pp_truthy(json_obj_get(req, "argv_external")))
        return json_string("--external-supervisor");
    if (pp_truthy(json_obj_get(req, "env_error"))) return json_null();   /* unreadable: no marker proven */
    if (!env || env->type != JSON_OBJECT) env = NULL;
    if (env && pp_truthy(json_obj_get(env, "HERMES_S6_SUPERVISED_CHILD")))
        return json_string("s6");
    if (pp_truthy(unit)) {
        sb_puts(&b, "systemd unit ");
        sb_puts(&b, unit_text ? unit_text : "");
        out_text = sb_take(&b);
        out = json_string(out_text ? out_text : "");
        free(out_text);
        return out;
    }
    xpc = env ? json_obj_get(env, "XPC_SERVICE_NAME") : NULL;
    if (!xpc) snprintf(text, sizeof(text), "0");              /* env.get(key, "0") */
    else pp_str_text(text, sizeof(text), xpc);
    /* launchd names terminal apps' shells application.<bundle id>: not KeepAlive jobs. */
    if (strcmp(text, "") != 0 && strcmp(text, "0") != 0
            && strncmp(text, "application.", 12) != 0) {
        sb_puts(&b, "launchd job ");
        sb_puts(&b, text);
        out_text = sb_take(&b);
        out = json_string(out_text ? out_text : "");
        free(out_text);
        return out;
    }
    return json_null();
}

/* _left_running(): drop the runtimes whose stop would kill this updater. */
json_t *w30_updatecmdp_fa52e9_p2_left_running(const json_t *req) {
    const json_t *units = json_obj_get(req, "units");
    const json_t *jobs = json_obj_get(req, "jobs");
    const json_t *xpc = json_obj_get(req, "xpc");
    const char *xpc_value = (xpc && xpc->type != JSON_NULL) ? pp_text(xpc) : NULL;
    int needs_sudo = pp_truthy(json_obj_get(req, "needs_sudo"));
    int sudo_ok = pp_truthy(json_obj_get(req, "sudo_ok"));
    json_t *keep_units = json_array(), *keep_jobs = json_array(), *notices = json_array();
    json_t *out = json_object();
    size_t i, n;

    if (!keep_units || !keep_jobs || !notices || !out) {
        json_free(keep_units); json_free(keep_jobs); json_free(notices); json_free(out);
        return json_null();
    }
    if (units && units->type == JSON_ARRAY) {
        n = json_len(units);
        for (i = 0; i < n; i++) {
            const json_t *u = json_get(units, i);
            const char *scope, *name;
            pp_sb b = {NULL, 0, 0};
            char *msg;
            if (!u || u->type != JSON_OBJECT) continue;
            scope = pp_text(json_obj_get(u, "scope"));
            name = pp_text(json_obj_get(u, "unit"));
            if (!name) name = "";
            if (scope && strcmp(scope, "system") == 0 && needs_sudo && !sudo_ok) {
                sb_printf(&b, "  \xe2\x86\xb7 %s keeps running until after the update "
                              "(stopping a system unit needs root or passwordless sudo)", name);
                msg = sb_take(&b);
                json_append(notices, json_string(msg ? msg : ""));
                free(msg);
            } else if (pp_truthy(json_obj_get(u, "escape_ok"))) {
                json_append(keep_units, json_string(name));
            } else {
                sb_printf(&b, "  \xe2\x86\xb7 %s keeps running until after the update "
                              "(this update could not prove it runs outside that unit's cgroup)", name);
                msg = sb_take(&b);
                json_append(notices, json_string(msg ? msg : ""));
                free(msg);
            }
        }
    }
    if (jobs && jobs->type == JSON_ARRAY) {
        n = json_len(jobs);
        for (i = 0; i < n; i++) {
            const json_t *j = json_get(jobs, i);
            const char *label;
            int ancestor;
            if (!j || j->type != JSON_OBJECT) continue;
            /* The label is carried AS IS (a null stays null) and compared only
             * when it is a string: None == "0" is False, and a notice naming a
             * missing label renders Python's "None". */
            {
                const json_t *label_node = json_obj_get(j, "label");
                label = (label_node && label_node->type == JSON_STRING) ? pp_text(label_node) : NULL;
            }
            ancestor = pp_truthy(json_obj_get(j, "ancestor"));
            if (ancestor || (xpc_value && label && strcmp(xpc_value, label) == 0)) {
                pp_sb b = {NULL, 0, 0};
                char *msg;
                sb_printf(&b, "  \xe2\x86\xb7 %s keeps running until after the update "
                              "(this update runs inside its launchd job)", label ? label : "None");
                msg = sb_take(&b);
                json_append(notices, json_string(msg ? msg : ""));
                free(msg);
            } else {
                json_append(keep_jobs, label ? json_string(label) : json_null());
            }
        }
    }
    json_set(out, "keep_units", keep_units);
    json_set(out, "keep_jobs", keep_jobs);
    json_set(out, "notices", notices);
    return out;
}

/* _abort_unsettled(): the --force refusal, and the token it leaves owed. */
json_t *w30_updatecmdp_fa52e9_p2_abort_unsettled(const json_t *req) {
    const json_t *why_value = json_obj_get(req, "why");
    const json_t *token_in = json_obj_get(req, "token");
    json_t *token = token_in && token_in->type == JSON_OBJECT ? json_copy(token_in) : json_object();
    const json_t *units;
    pp_sb b = {NULL, 0, 0};
    char *msg, why_text[1024];
    json_t *out;

    if (!token) return json_null();
    if (why_value) pp_str_text(why_text, sizeof(why_text), why_value);
    else snprintf(why_text, sizeof(why_text), "None");

    json_set(token, "resume_needed", json_bool(false));
    units = json_obj_get(token, "posix_units");
    if (units && units->type == JSON_ARRAY) {
        size_t i, n = json_len(units);
        for (i = 0; i < n; i++) {
            json_t *u = json_get(units, i);
            const char *kind;
            if (!u || u->type != JSON_OBJECT) continue;
            kind = pp_text(json_obj_get(u, "kind"));
            if (kind && strcmp(kind, "systemd") == 0)
                json_set(u, "stop_unsettled", json_bool(true));
        }
    }
    json_set(token, "resume_deferred", why_value ? json_copy(why_value) : json_null());

    sb_printf(&b, "--force could not finish stopping the gateways: %s. The update stopped before "
                  "changing the checkout; the stopped gateways restart at the next `hermes` command once "
                  "their stop has completed, then run `hermes update --force` again", why_text);
    msg = sb_take(&b);

    out = json_object();
    json_set(out, "message", json_string(msg ? msg : ""));
    json_set(out, "token", token);
    free(msg);
    return out;
}

/* SUPERVISOR_MARKER_ENV (gateway_runtime_start): the identity variables a
 * detached replay must not carry. */
static const char *const PP_SUPERVISOR_MARKERS[] = {
    "INVOCATION_ID", "JOURNAL_STREAM",
    "XPC_SERVICE_NAME", "LAUNCHD_SOCKET", "HERMES_LAUNCHD_LABEL",
    "HERMES_GATEWAY_EXTERNAL_SUPERVISOR", "HERMES_SUPERVISED_CHILD",
    "NOTIFY_SOCKET", "WATCHDOG_USEC", "WATCHDOG_PID",
};

#define PP_PIN_ENV "HERMES_GATEWAY_PROFILE_PIN"

/* carry_gateway_profile_pin(): a pinning supervisor marker leaves the profile
 * pin behind while it is stripped. */
static void pp_carry_pin(json_t *env) {
    const json_t *child = json_obj_get(env, "HERMES_SUPERVISED_CHILD");
    const json_t *ext = json_obj_get(env, "HERMES_GATEWAY_EXTERNAL_SUPERVISOR");
    char text[256];
    size_t i, n;

    if (pp_truthy(child)) { json_set(env, PP_PIN_ENV, json_string("1")); return; }
    if (!ext) text[0] = '\0';                      /* env.get(key, "") */
    else pp_str_text(text, sizeof(text), ext);
    i = 0;
    n = strlen(text);
    while (i < n && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n'
                     || text[i] == '\r' || text[i] == '\f' || text[i] == '\v')) i++;
    while (n > i && (text[n - 1] == ' ' || text[n - 1] == '\t' || text[n - 1] == '\n'
                     || text[n - 1] == '\r' || text[n - 1] == '\f' || text[n - 1] == '\v')) n--;
    memmove(text, text + i, n - i);
    text[n - i] = '\0';
    for (i = 0; text[i]; i++)
        if (text[i] >= 'A' && text[i] <= 'Z') text[i] = (char)(text[i] - 'A' + 'a');
    if (strcmp(text, "1") == 0 || strcmp(text, "true") == 0
            || strcmp(text, "yes") == 0 || strcmp(text, "on") == 0)
        json_set(env, PP_PIN_ENV, json_string("1"));
}

/* _bare_replay_env(): strip the updater's supervisor markers, keep the pin. */
json_t *w30_updatecmdp_fa52e9_p2_bare_replay_env(const json_t *req) {
    const json_t *env_in = json_obj_get(req, "env");
    json_t *env = env_in && env_in->type == JSON_OBJECT ? json_copy(env_in) : json_object();
    size_t i;

    if (!env) return json_null();
    pp_carry_pin(env);
    if (pp_truthy(json_obj_get(env, "INVOCATION_ID")))
        json_set(env, PP_PIN_ENV, json_string("1"));
    for (i = 0; i < sizeof(PP_SUPERVISOR_MARKERS) / sizeof(PP_SUPERVISOR_MARKERS[0]); i++)
        json_obj_del(env, PP_SUPERVISOR_MARKERS[i]);
    json_obj_del(env, "HERMES_UPDATE_PAUSED");
    json_obj_del(env, "_HERMES_GATEWAY");
    return env;
}

/* _gateway_on_home(): the gateway already serving *home*, never the excluded
 * PID; a gateway someone started meanwhile is adopted, not duplicated. */
json_t *w30_updatecmdp_fa52e9_p2_gateway_on_home(const json_t *req) {
    const json_t *pid = json_obj_get(req, "home_pid");
    const json_t *exclude = json_obj_get(req, "exclude");
    long long n = 0, ex = 0;
    size_t i, len;

    if (pp_truthy(json_obj_get(req, "error"))) return json_null();   /* unreadable: nothing proven */
    if (!pp_truthy(pid)) return json_null();
    if (!pp_to_int(pid, &n)) return json_null();
    if (exclude && exclude->type == JSON_ARRAY) {
        len = json_len(exclude);
        for (i = 0; i < len; i++)
            if (pp_to_int(json_get(exclude, i), &ex) && ex == n) return json_null();
    }
    return json_number((double)n);
}
