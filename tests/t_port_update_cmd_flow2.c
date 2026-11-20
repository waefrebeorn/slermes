/*
 * t_port_update_cmd_flow2.c — oracle harness for the REMAINING UNCLAIMED
 * update-pipeline FLOW cores of hermes_cli/update_cmd.py ported in
 * src/cli/update_cmd_flow2.c.
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/update_cmd_flow2.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}} (plus "state" for the
 * mutation flows complete_source_update / finish_already_up_to_date /
 * apply_pulled_update).
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL (last
 * repeats, empty array -> None). tests/oracle_update_cmd_flow2.py drives the LIVE
 * Python with the same scripts and compares the ordered decision sequence +
 * return value (raise envelopes included).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/update_cmd_flow2.c"

typedef struct {
    const json_t *in;
    uc2_trace_t *trace;
    json_t *counters;
} uc2_script_ctx_t;

static json_t *uc2_args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(out, args->c.keys[i], json_copy(v));
        }
    }
    return out;
}

static json_t *uc2_script_call(void *vctx, const char *name, const json_t *args) {
    uc2_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = uc2_xsf("unscripted seam: %s", name);
        result = uc2_raise_spec("RuntimeError", msg);
        free(msg);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", uc2_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    uc2_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)

/* Strip kind-specific extras from a propagated scripted raise at the ret
 * boundary, keeping __raise__/msg (== the oracle drive's BaseException catch). */
static json_t *uc2_norm_raise_ret(json_t *ret) {
    if (!uc2_is_raise(ret)) return ret;
    json_t *n = json_object();
    json_set(n, "__raise__", json_string(uc2_raise_class(ret)));
    json_set(n, "msg", json_string(json_get_str(ret, "msg", "")));
    json_free(ret);
    return n;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    uc2_trace_t trace = {NULL};
    json_t *counters = json_object();
    uc2_script_ctx_t ctx = {in, &trace, counters};
    uc2_seams_t seams = {&ctx, uc2_script_call};
    const uc2_seams_t *sp = &seams;
    json_t *ret = NULL;
    json_t *workstate = NULL;
    bool echo_state = false;

    if (!strcmp(func, "source_completion_request")) {
        ret = uc2_source_completion_request(sp, J("opts"), J("plan"), J("snapshot_id"),
                                            J("windows_resume"), B("desktop", false), B("gateway_mode", false));
    } else if (!strcmp(func, "complete_source_update")) {
        const json_t *rq = J("request");
        workstate = (rq && rq->type != JSON_NULL) ? json_copy(rq) : NULL;
        ret = uc2_complete_source_update(sp, workstate);
        echo_state = true;
    } else if (!strcmp(func, "reconcile_diverged_checkout")) {
        ret = uc2_reconcile_diverged_checkout(sp, J("git_cmd"), json_get_str(in, "branch", ""),
                                              J("pre_pull_sha"), J("target_ref"));
    } else if (!strcmp(func, "rollback_if_pulled_syntax_error")) {
        ret = uc2_rollback_if_pulled_syntax_error(sp, J("git_cmd"), J("pre_pull_sha"),
                                                  J("rollback_branch"), J("token"));
    } else if (!strcmp(func, "roll_back_broken_update")) {
        ret = uc2_roll_back_broken_update(sp, J("git_cmd"), J("pre_pull_sha"),
                                          json_get_str(in, "failing_path", ""), J("syntax_error"),
                                          J("rollback_branch"),
                                          json_get_str(in, "header", UC2_CROSS " Pulled code has a syntax error in a critical file:"),
                                          J("token"), json_get_str(in, "stop_class", "syntax_rollback"));
    } else if (!strcmp(func, "move_checkout_to")) {
        ret = uc2_move_checkout_to(sp, J("git_cmd"), json_get_str(in, "branch", ""),
                                   json_get_str(in, "merge_ref", ""), json_get_str(in, "target_sha", ""),
                                   J("pre_pull_sha"));
    } else if (!strcmp(func, "pull_updates")) {
        ret = uc2_pull_updates(sp, J("git_cmd"), json_get_str(in, "branch", ""), J("auto_stash_ref"),
                             B("prompt_for_restore", false), J("gw_input_fn"), B("discard_local_changes", false),
                             B("keep_stash", false), J("target_ref"), J("pre_sync_sha"), J("rollback_branch"),
                             B("sync_upstream", false), B("assume_yes", false), B("in_place_update", false),
                             J("token"), J("expected_sha"));
    } else if (!strcmp(func, "switch_branch_at_commit_point")) {
        ret = uc2_switch_branch_at_commit_point(sp, J("git_cmd"), json_get_str(in, "branch", ""),
                                                json_get_str(in, "target_ref", ""), J("pre"), J("stash"));
    } else if (!strcmp(func, "refuse_before_commit_point")) {
        ret = uc2_refuse_before_commit_point(sp, J("git_cmd"), json_get_str(in, "target_ref", ""), J("token"));
    } else if (!strcmp(func, "exit_after_failed_branch_switch")) {
        ret = uc2_exit_after_failed_branch_switch(sp, J("git_cmd"), json_get_str(in, "branch", ""),
                                                  J("track_result"), J("auto_stash_ref"), J("gw_input_fn"), J("token"));
    } else if (!strcmp(func, "apply_parked_branch_guard")) {
        ret = uc2_apply_parked_branch_guard(sp, J("git_cmd"), json_get_str(in, "branch", ""),
                                            json_get_str(in, "current_branch", ""), B("switch_branch", false), J("token"));
    } else if (!strcmp(func, "prepare_checkout_for_update")) {
        ret = uc2_prepare_checkout_for_update(sp, J("git_cmd"), json_get_str(in, "branch", ""),
                                              json_get_str(in, "current_branch", ""), B("is_fork", false),
                                              B("assume_yes", false), B("gateway_mode", false), J("gw_input_fn"),
                                              B("switch_branch", false), J("target_ref"), J("token"));
    } else if (!strcmp(func, "resolve_update_options")) {
        ret = uc2_resolve_update_options(sp, J("args"), B("gateway_mode", false));
    } else if (!strcmp(func, "begin_update_receipt_and_plan")) {
        ret = uc2_begin_update_receipt_and_plan(sp);
    } else if (!strcmp(func, "record_update_initiator")) {
        ret = uc2_record_update_initiator(sp);
    } else if (!strcmp(func, "prepare_git_command")) {
        ret = uc2_prepare_git_command(sp, J("checkout_move"), json_get_str(in, "platform", "linux"));
    } else if (!strcmp(func, "handle_update_called_process_error")) {
        ret = uc2_handle_update_called_process_error(sp, J("e"), J("args"), B("gateway_mode", false),
                                                     B("had_desktop_app_before_update", false), J("target_sha"),
                                                     J("target_repository"), J("completion_request"));
    } else if (!strcmp(func, "finish_already_up_to_date")) {
        const json_t *rq = J("completion_request");
        workstate = (rq && rq->type != JSON_NULL) ? json_copy(rq) : NULL;
        ret = uc2_finish_already_up_to_date(sp, J("git_cmd"), json_get_str(in, "branch", ""),
                                            json_get_str(in, "current_branch", ""), J("plan"), J("gw_input_fn"),
                                            workstate, J("token"), J("held_message"));
        echo_state = true;
    } else if (!strcmp(func, "apply_pulled_update")) {
        const json_t *rq = J("completion_request");
        workstate = (rq && rq->type != JSON_NULL) ? json_copy(rq) : NULL;
        ret = uc2_apply_pulled_update(sp, J("git_cmd"), json_get_str(in, "branch", ""),
                                      J("movement_baseline"), J("plan"), J("token"), workstate);
        echo_state = true;
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    ret = uc2_norm_raise_ret(ret);
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    if (echo_state) json_set(out, "state", workstate ? json_copy(workstate) : json_object());
    uc2_trace_free(&trace);
    json_free(counters);
    if (workstate) json_free(workstate);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) > 0) {
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case: %s\n", err ? err : "not object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        /* json_escape_string has a 1-byte heap overflow (libjson, not owned
         * here) — use the bounds-safe serializer path instead. */
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    free(line);
    return 0;
}
