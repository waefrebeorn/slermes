/*
 * run_shutdown_flow.c — gateway shutdown lifecycle FLOW cores of
 * gateway/run_shutdown.py. See include/run_shutdown_flow.h.
 *
 * Decision sequences only; every live I/O enters through seams. Log seams
 * record the raw format string plus the argument array (no printf formatting),
 * matching the oracle's recording logger.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "run_shutdown_flow.h"

/* ── internal helpers ── */
static void emit_log(gsd_log_fn log, void *ctx, int level, const char *fmt, json_t *args) {
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
static json_t *args_int(int n) {
    json_t *x = json_array();
    json_append(x, json_number(n));
    return x;
}
static json_t *args_empty(void) { return json_array(); }

/* ── 1. _exit_with_failure_verdict ── */
bool gsd_flow_exit_with_failure_verdict(const json_t *runner, const gsd_failure_seams_t *s) {
    if (!gflow_truth(gflow_obj_get(runner, "should_exit_with_failure"))) return false;
    const json_t *reason = gflow_obj_get(runner, "exit_reason");
    if (gflow_truth(reason)) {
        emit_log(s ? s->log : NULL, s ? s->ctx : NULL, 40,
                 "Gateway exiting with failure: %s", args_str(gflow_str(reason)));
    }
    return true;
}

/* ── 2. _resolve_gateway_exit_verdict ── */
#define EXIT_SIGNAL_FMT \
    "Exiting with code 1 (signal-initiated shutdown without restart request) so the " \
    "service manager can revive the gateway."
#define EXIT_SERVICE_FMT \
    "Exiting with code %d (service-restart requested) so the service manager relaunches " \
    "the gateway."

gsd_exit_verdict_t gsd_flow_resolve_gateway_exit_verdict(const json_t *runner,
                                                         bool signal_initiated_shutdown,
                                                         const gsd_resolve_seams_t *s) {
    gsd_exit_verdict_t r = { false, 0, false };
    void *ctx = s ? s->ctx : NULL;
    gsd_log_fn log = s ? s->log : NULL;
    bool failure = s && s->exit_with_failure_verdict
                   ? s->exit_with_failure_verdict(ctx, runner) : false;
    if (failure) { r.verdict = false; return r; }
    const json_t *ec = gflow_obj_get(runner, "exit_code");
    if (!gflow_is_null(ec)) {
        r.exited = true;
        r.exit_code = (int)(ec->type == JSON_NUMBER ? ec->num_val : 0);
        return r;
    }
    if (signal_initiated_shutdown && !gflow_truth(gflow_obj_get(runner, "_restart_requested"))) {
        emit_log(log, ctx, 20, EXIT_SIGNAL_FMT, args_empty());
        r.verdict = false;
        return r;
    }
    if (gflow_truth(gflow_obj_get(runner, "_restart_via_service"))) {
        emit_log(log, ctx, 20, EXIT_SERVICE_FMT, args_int(75));
        r.exited = true;
        r.exit_code = 75;
        return r;
    }
    r.verdict = true;
    return r;
}

/* ── 3. _notice_allowed ── */
bool gsd_flow_notice_allowed(const json_t *config_platforms, const char *platform_value,
                             const char *what, const json_t *platform_cfg,
                             const gsd_notice_seams_t *s) {
    bool allowed;
    if (gflow_is_null(platform_cfg)) {
        /* _restart_notification_allowed: config.platforms.get(platform). */
        const json_t *pcfg = gflow_obj_get(config_platforms, platform_value);
        allowed = gflow_is_null(pcfg)
                  || gflow_truth(gflow_obj_get(pcfg, "gateway_restart_notification"));
    } else {
        allowed = gflow_truth(gflow_obj_get(platform_cfg, "gateway_restart_notification"));
    }
    if (allowed) return true;
    emit_log(s ? s->log : NULL, s ? s->ctx : NULL, 20,
             "Shutdown notification suppressed for %s: %s has gateway_restart_notification=false",
             args_str2(what, platform_value));
    return false;
}

/* ── 4. _shutdown_interrupt_reason ── */
const char *gsd_flow_shutdown_interrupt_reason(bool restart_requested) {
    return restart_requested ? "Gateway restarting" : "Gateway shutting down";
}

/* ── 5. _quiet_step ── */
json_t *gsd_flow_quiet_step(const char *label, const gsd_quiet_seams_t *s) {
    bool raised = false;
    const char *err = NULL;
    json_t *ret = s && s->fn ? s->fn(s->ctx, &raised, &err) : NULL;
    if (raised) {
        emit_log(s ? s->log : NULL, s ? s->ctx : NULL, 10, "%s: %s",
                 args_str2(label, err ? err : ""));
        return json_null();
    }
    return ret ? json_copy(ret) : json_null();
}

/* ── 6. _scale_to_zero_status ── */
void gsd_flow_scale_to_zero_status(const char *state, const char *fail_msg,
                                   const gsd_stz_seams_t *s) {
    bool raised = false;
    if (s && s->update_runtime_status)
        s->update_runtime_status(s->ctx, state, &raised);
    if (raised)
        emit_log(s ? s->log : NULL, s ? s->ctx : NULL, 10, fail_msg, args_empty());
}
