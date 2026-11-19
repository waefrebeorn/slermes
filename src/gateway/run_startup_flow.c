/*
 * run_startup_flow.c — gateway startup lifecycle FLOW cores of
 * gateway/run_startup.py. See include/run_startup_flow.h.
 *
 * Decision sequences only; every live I/O enters through seams. Runner state
 * is injected json_t mutated in place. Log seams record the raw format string
 * plus the argument array (no printf formatting), matching the oracle's
 * recording logger which captures fmt + args before %-formatting.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "run_startup_flow.h"

/* ── internal helpers ── */

/* Emit a log event; takes ownership of args (frees after the borrowed call). */
static void emit_log(gsu_log_fn log, void *ctx, int level, const char *fmt, json_t *args) {
    if (log) log(ctx, level, fmt, args);
    json_free(args);
}
static json_t *args_str(const char *a) {
    json_t *x = json_array();
    json_append(x, json_string(a ? a : ""));
    return x;
}
static json_t *args_str2(const char *a, const char *b) {
    json_t *x = json_array();
    json_append(x, json_string(a ? a : ""));
    json_append(x, json_string(b ? b : ""));
    return x;
}
static json_t *args_count_join(int n, const char *joined) {
    json_t *x = json_array();
    json_append(x, json_number(n));
    json_append(x, json_string(joined ? joined : ""));
    return x;
}
static json_t *args_int(int n) {
    json_t *x = json_array();
    json_append(x, json_number(n));
    return x;
}
static json_t *args_empty(void) { return json_array(); }

/* "; ".join(array of strings). malloc'd; caller frees. */
static char *join_semicolon(const json_t *arr) {
    size_t cap = 1;
    size_t n = gflow_is_arr(arr) ? arr->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = gflow_str(arr->c.items[i]);
        cap += (s ? strlen(s) : 0) + 2;
    }
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        const char *s = gflow_str(arr->c.items[i]);
        if (i) strcat(out, "; ");
        strcat(out, s ? s : "");
    }
    return out;
}
static int arr_len(const json_t *arr) {
    return gflow_is_arr(arr) ? (int)arr->c.count : 0;
}

/* ── 1. _serving_state ── */
const char *gsu_flow_serving_state(const json_t *st) {
    return gflow_truth(gflow_obj_get(st, "_startup_parked_platforms")) ? "degraded" : "running";
}

/* ── 2. _startup_should_abort ── */
bool gsu_flow_startup_should_abort(const json_t *st, const gsu_abort_seams_t *s) {
    if (gflow_truth(gflow_obj_get(st, "_restart_requested"))) return true;
    if (gflow_truth(gflow_obj_get(st, "_draining"))) return true;
    return s && s->shutdown_event_is_set ? s->shutdown_event_is_set(s->ctx) : false;
}

/* ── 3. _resume_owner_authorized ── */
bool gsu_flow_resume_owner_authorized(const char *session_key, const json_t *source,
                                      const gsu_resume_seams_t *s) {
    bool raised = false;
    const char *err = NULL;
    bool ok = s && s->is_user_authorized
              ? s->is_user_authorized(s->ctx, source, &raised, &err) : false;
    if (raised) {
        emit_log(s ? s->log : NULL, s ? s->ctx : NULL, 30,
                 "Skipping auto-resume for %s: authorization check failed: %s",
                 args_str2(session_key, err ? err : ""));
        return false;
    }
    if (ok) return true;
    emit_log(s ? s->log : NULL, s ? s->ctx : NULL, 30,
             "Skipping auto-resume for %s: session owner is no longer authorized "
             "under the current allowlist",
             args_str(session_key));
    return false;
}

/* ── 4. _park_last_fatal_platform ── */
#define PARK_FMT \
    "%s is parked after a non-retryable error (fix it, then `hermes gateway restart`). " \
    "The gateway is DEGRADED — it serves local clients and cron with no " \
    "messaging platform. %s: %s"

bool gsu_flow_park_last_fatal_platform(json_t *st, const json_t *adapter,
                                       const gsu_park_seams_t *s) {
    if (gflow_truth(gflow_obj_get(adapter, "fatal_error_retryable"))) return false;
    const json_t *code_v = gflow_obj_get(adapter, "fatal_error_code");
    const char *code = gflow_str(code_v);
    bool conflict = s && s->is_global_startup_conflict
                    ? s->is_global_startup_conflict(s->ctx, code) : false;
    if (conflict) return false;
    json_set(st, "_startup_parked_platforms", json_bool(true));
    bool ext = gflow_truth(gflow_obj_get(st, "_external_drain_active"));
    bool draining = gflow_truth(gflow_obj_get(st, "_draining"));
    bool running = gflow_truth(gflow_obj_get(st, "_running"));
    if (!(ext || draining || !running)) {
        if (s && s->update_runtime_status)
            s->update_runtime_status(s->ctx, gsu_flow_serving_state(st));
    }
    const char *pval = gflow_str(gflow_obj_get(gflow_obj_get(adapter, "platform"), "value"));
    const json_t *msg_v = gflow_obj_get(adapter, "fatal_error_message");
    const char *msg = gflow_str(msg_v);
    json_t *args = json_array();
    json_append(args, json_string(pval ? pval : ""));
    json_append(args, json_string(gflow_truth(code_v) ? code : "unknown"));
    json_append(args, json_string(gflow_truth(msg_v) ? msg : "unknown error"));
    emit_log(s ? s->log : NULL, s ? s->ctx : NULL, 40, PARK_FMT, args);
    return true;
}

/* ── 5. _start_handle_no_connections ── */
#define NOCONN_PARKED_REMAINING_FMT \
    "%d configured platform(s) failed to start and are parked (fix the reported error, " \
    "then `hermes gateway restart`): %s. The gateway is DEGRADED — it serves " \
    "the remaining platform(s) with those unserved."
#define NOCONN_CONFLICT_FMT "Gateway hit a non-retryable startup conflict: %s"
#define NOCONN_PARKED_LOCAL_FMT \
    "%d configured platform(s) failed to start and are parked (fix the reported error, " \
    "then `hermes gateway restart`): %s. The gateway is DEGRADED — it serves " \
    "local clients and cron with no messaging platform."
#define NOCONN_MIXED_FMT \
    "%d platform(s) fatally misconfigured and parked: %s. Staying alive so retryable " \
    "platforms can recover."
#define NOCONN_RETRY_FMT \
    "Gateway started with no connected platforms — %d platform(s) queued for " \
    "retry: %s"
#define NOCONN_NONE_FMT \
    "No adapter could be created for any of the %d configured platform(s). Check that " \
    "required dependencies are installed and credentials are set. Gateway will continue " \
    "for cron job execution; platforms whose plugin may still register are queued for " \
    "background retry."

bool gsu_flow_start_handle_no_connections(json_t *st, int connected_count,
                                          int enabled_platform_count,
                                          const json_t *retryable,
                                          const json_t *nonretryable,
                                          const gsu_noconn_seams_t *s) {
    void *ctx = s ? s->ctx : NULL;
    gsu_log_fn log = s ? s->log : NULL;
    int n_non = arr_len(nonretryable);
    int n_ret = arr_len(retryable);
    char *non_join = join_semicolon(nonretryable);
    char *ret_join = join_semicolon(retryable);

    if (connected_count != 0) {
        if (n_non > 0) {
            json_set(st, "_startup_parked_platforms", json_bool(true));
            emit_log(log, ctx, 40, NOCONN_PARKED_REMAINING_FMT,
                     args_count_join(n_non, non_join));
        }
        free(non_join); free(ret_join);
        return false;
    }
    if (n_non > 0 && n_ret == 0) {
        if (gflow_truth(gflow_obj_get(st, "_startup_ownership_conflict"))) {
            emit_log(log, ctx, 40, NOCONN_CONFLICT_FMT, args_str(non_join));
            if (s && s->startup_fail_fatal_config)
                s->startup_fail_fatal_config(ctx, non_join);
            free(non_join); free(ret_join);
            return true;
        }
        json_set(st, "_startup_parked_platforms", json_bool(true));
        emit_log(log, ctx, 40, NOCONN_PARKED_LOCAL_FMT, args_count_join(n_non, non_join));
        free(non_join); free(ret_join);
        return false;
    }
    if (n_non > 0) {
        json_set(st, "_startup_parked_platforms", json_bool(true));
        emit_log(log, ctx, 40, NOCONN_MIXED_FMT, args_count_join(n_non, non_join));
    }
    if (enabled_platform_count <= 0) {
        emit_log(log, ctx, 30, "No messaging platforms enabled.", args_empty());
        emit_log(log, ctx, 20, "Gateway will continue running for cron job execution.",
                 args_empty());
        free(non_join); free(ret_join);
        return false;
    }
    if (n_ret > 0) {
        emit_log(log, ctx, 30, NOCONN_RETRY_FMT, args_count_join(n_ret, ret_join));
        if (s && s->write_runtime_status_quiet)
            s->write_runtime_status_quiet(ctx, "degraded", NULL);
    }
    emit_log(log, ctx, 30, NOCONN_NONE_FMT, args_int(enabled_platform_count));
    free(non_join); free(ret_join);
    return false;
}

/* ── 6. _startup_fail_fatal_config ── */
void gsu_flow_startup_fail_fatal_config(json_t *st, const char *reason,
                                        const gsu_fatal_seams_t *s) {
    if (s && s->write_runtime_status_quiet)
        s->write_runtime_status_quiet(s->ctx, "startup_failed", reason);
    json_set(st, "_exit_code", json_number(78));
    if (s && s->request_clean_exit)
        s->request_clean_exit(s->ctx, reason);
    json_set(st, "_startup_restore_in_progress", json_bool(false));
}
