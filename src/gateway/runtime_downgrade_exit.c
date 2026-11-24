/*
 * runtime_downgrade_exit.c — C home for gateway/runtime_downgrade_exit.py.
 * Faithful port of the pure decision helpers and the watcher's confirmation
 * state machine. The probes (skew, update lock, predates) are injected as
 * per-poll decision inputs; all branch order, counters, reason text and exit
 * code mirror the live Python exactly.
 * PoP: w18_runtimedow_f303ab_downgrade_reason @ gateway/runtime_downgrade_exit.py:downgrade_reason
 * PoP: w18_runtimedow_f303ab_tree_predates @ gateway/runtime_downgrade_exit.py:tree_predates_runtime
 * PoP: w18_runtimedow_f303ab_watcher @ gateway/runtime_downgrade_exit.py:runtime_downgrade_watcher
 * PoP: w18_runtimedow_f303ab_constants @ gateway/runtime_downgrade_exit.py:<module constants>
 */
#include "noc_w18_runtimedow_f303ab.h"
#include <stdio.h>
#include <string.h>

/* Core of downgrade_reason: writes the exact reason line into buf and returns 1,
 * or returns 0 when no downgrade can be concluded. */
static int reason_text(int has_skew, const json_t *skew, int update_in_progress,
                       int predates, char *buf, size_t n) {
    if (!has_skew || !skew || skew->type != JSON_ARRAY || json_len(skew) == 0
            || update_in_progress || !predates) return 0;
    json_t *a = json_get(skew, 0);
    json_t *b = json_len(skew) > 1 ? json_get(skew, 1) : NULL;
    const char *sa = (a && a->type == JSON_STRING) ? a->str_val : "";
    const char *sb = (b && b->type == JSON_STRING) ? b->str_val : "";
    snprintf(buf, n,
        "This gateway loaded %s, but its checkout is now at %s, a release that predates the "
        "gateway runtime. Downgrading is unsupported; stopping so the service manager starts "
        "the older release's own gateway.", sa, sb);
    return 1;
}

json_t *w18_runtimedow_f303ab_downgrade_reason(int has_skew, json_t *skew,
                                              int update_in_progress, int predates) {
    char buf[4096];
    if (!reason_text(has_skew, skew, update_in_progress, predates, buf, sizeof(buf)))
        return json_null();
    return json_string(buf);
}

/* bool(sources) and not defines_runtime(sources); defines_runtime is
 * any(MARKER in source for source in sources). A read error models as no sources. */
json_t *w18_runtimedow_f303ab_tree_predates(json_t *sources) {
    int any_source = 0, defines = 0;
    if (sources && sources->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(sources); i++) {
            any_source = 1;
            json_t *s = json_get(sources, i);
            if (s && s->type == JSON_STRING && s->str_val
                    && strstr(s->str_val, W18_RUNTIMEDOW_MARKER) != NULL) {
                defines = 1;
                break;
            }
        }
    }
    return json_bool(any_source && !defines);
}

/* The runtime_downgrade_watcher confirmation state machine.
 * Each element of `polls` supplies: skew (array), update_busy, update_error,
 * predates. max_polls < 0 means None. `running` models runner._running. */
json_t *w18_runtimedow_f303ab_watcher(int running, int max_polls, json_t *polls) {
    json_t *out = json_object();
    int seen = 0, count = 0, stopped = 0;
    char last_reason[4096];
    last_reason[0] = '\0';
    int have_reason = 0;
    size_t total = (polls && polls->type == JSON_ARRAY) ? json_len(polls) : 0;
    for (size_t i = 0; i < total; i++) {
        if (!running) break;
        if (max_polls >= 0 && count >= max_polls) break;
        count++;
        json_t *spec = json_get(polls, i);
        int busy = spec ? json_get_bool(spec, "update_busy", false) : 0;
        if (spec && json_get_bool(spec, "update_error", false)) busy = 1; /* unreadable lock -> busy */
        json_t *skew = spec ? json_obj_get(spec, "skew") : NULL;
        int predates = spec ? json_get_bool(spec, "predates", false) : 0;
        char buf[4096];
        if (reason_text(1, skew, busy, predates, buf, sizeof(buf))) {
            seen++;
            have_reason = 1;
            memcpy(last_reason, buf, sizeof(last_reason));
        } else {
            seen = 0;
            have_reason = 0;
        }
        if (seen >= W18_RUNTIMEDOW_CONFIRMATIONS) {
            stopped = 1;
            break;
        }
    }
    json_set(out, "polls", json_number((double)count));
    /* Python assigns runner._exit_reason only on a confirmed downgrade, so a
     * single unconfirmed reason read is never reported. */
    json_set(out, "reason", (stopped && have_reason) ? json_string(last_reason) : json_null());
    json_set(out, "exit_code", stopped ? json_number((double)W18_RUNTIMEDOW_SERVICE_RESTART_EXIT_CODE) : json_null());
    json_set(out, "stop_scheduled", json_bool(stopped));
    return out;
}

json_t *w18_runtimedow_f303ab_constants(void) {
    json_t *out = json_object();
    json_set(out, "POLL_S", json_number(W18_RUNTIMEDOW_POLL_S));
    json_set(out, "CONFIRMATIONS", json_number((double)W18_RUNTIMEDOW_CONFIRMATIONS));
    json_set(out, "SERVICE_RESTART_EXIT_CODE",
             json_number((double)W18_RUNTIMEDOW_SERVICE_RESTART_EXIT_CODE));
    json_set(out, "MARKER", json_string(W18_RUNTIMEDOW_MARKER));
    return out;
}

