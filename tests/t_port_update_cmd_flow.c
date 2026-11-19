/*
 * t_port_update_cmd_flow.c — oracle harness for the UNCLAIMED update-pipeline
 * FLOW cores of hermes_cli/update_cmd.py ported in src/cli/update_cmd_flow.c.
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/update_cmd_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}} (plus "state" for
 * the mutation flow settle_windows_resume).
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None). tests/oracle_update_cmd_flow.py drives
 * the LIVE Python with the same scripts and compares the ordered decision
 * sequence + return value (raise envelopes included).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/update_cmd_flow.c"

typedef struct {
    const json_t *in;
    uc_trace_t *trace;
    json_t *counters;
} uc_script_ctx_t;

static json_t *uc_args_drop_null(const json_t *args) {
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

static json_t *uc_script_call(void *vctx, const char *name, const json_t *args) {
    uc_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        /* fail-closed: an unscripted seam raises RuntimeError on both sides */
        char *msg = uc_xsf("unscripted seam: %s", name);
        result = uc_raise_spec("RuntimeError", msg);
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
    json_set(e, "args", uc_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    uc_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

/* The oracle drive's BaseException catch builds exactly {"__raise__": cls,
 * "msg": str(exc)}. A propagated scripted raise may carry extra fields
 * (cmd/returncode/stderr) used by an except branch; strip them at the ret
 * boundary, keeping the msg field (which equals str(exc) for every fixture
 * raise that propagates). Built raises already carry only __raise__/msg. */
static json_t *uc_norm_raise_ret(json_t *ret) {
    if (!uc_is_raise(ret)) return ret;
    json_t *n = json_object();
    json_set(n, "__raise__", json_string(uc_raise_class(ret)));
    json_set(n, "msg", json_string(json_get_str(ret, "msg", "")));
    json_free(ret);
    return n;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    uc_trace_t trace = {NULL};
    json_t *counters = json_object();
    uc_script_ctx_t ctx = {in, &trace, counters};
    uc_seams_t seams = {&ctx, uc_script_call};
    const uc_seams_t *sp = &seams;
    json_t *ret = NULL;
    json_t *workstate = NULL;
    bool echo_state = false;

    if (!strcmp(func, "get_default_hermes_root")) {
        ret = uc_get_default_hermes_root(sp);
    } else if (!strcmp(func, "ensure_venv_pip")) {
        ret = uc_ensure_venv_pip(sp);
    } else if (!strcmp(func, "pip_install_prefix")) {
        ret = uc_pip_install_prefix(sp);
    } else if (!strcmp(func, "updates_config")) {
        ret = uc_updates_config(sp);
    } else if (!strcmp(func, "record_update_step")) {
        ret = uc_record_update_step(sp, json_get_str(in, "step", ""),
                                    json_get_bool(in, "ok", false),
                                    json_get_str(in, "detail", ""));
    } else if (!strcmp(func, "record_update_skip")) {
        ret = uc_record_update_skip(sp, json_get_str(in, "step", ""),
                                    json_get_str(in, "reason", ""));
    } else if (!strcmp(func, "record_pre_update_backup_outcome")) {
        ret = uc_record_pre_update_backup_outcome(sp, J("args"), J("snapshot_id"));
    } else if (!strcmp(func, "record_snapshot_stage")) {
        ret = uc_record_snapshot_stage(sp, J("args"), J("snapshot_id"));
    } else if (!strcmp(func, "finalize_receipt")) {
        ret = uc_finalize_receipt(sp, json_get_str(in, "status", ""),
                                  json_get_str(in, "debug_message", ""));
    } else if (!strcmp(func, "git_run")) {
        ret = uc_git_run(sp, J("git_cmd"), J("args"), J("cwd"),
                         json_get_bool(in, "check", false),
                         json_get_bool(in, "network", false));
    } else if (!strcmp(func, "base_git_cmd")) {
        ret = uc_base_git_cmd(json_get_str(in, "platform", "linux"));
    } else if (!strcmp(func, "is_shallow_checkout")) {
        ret = uc_is_shallow_checkout(sp, J("git_cmd"));
    } else if (!strcmp(func, "tip_shas")) {
        ret = uc_tip_shas(sp, J("git_cmd"), json_get_str(in, "target_ref", ""),
                          json_get_str(in, "base", "HEAD"));
    } else if (!strcmp(func, "resolved_commit")) {
        ret = uc_resolved_commit(sp, J("git_cmd"), json_get_str(in, "ref", ""));
    } else if (!strcmp(func, "current_branch_name")) {
        ret = uc_current_branch_name(sp, J("git_cmd"), json_get_bool(in, "check", false));
    } else if (!strcmp(func, "landed_off_target")) {
        ret = uc_landed_off_target(sp, J("git_cmd"), json_get_str(in, "target_sha", ""),
                                   json_get_bool(in, "merged", true));
    } else if (!strcmp(func, "update_movement_baseline")) {
        ret = uc_update_movement_baseline(sp, J("git_cmd"), J("pre_pull_sha"),
                                          J("pre_sync_sha"), J("rollback_branch"), J("target_sha"));
    } else if (!strcmp(func, "missing_branch_stop")) {
        ret = uc_missing_branch_stop(sp, J("git_cmd"), json_get_str(in, "branch", ""));
    } else if (!strcmp(func, "refuse_unselected_head")) {
        ret = uc_refuse_unselected_head(sp, J("git_cmd"), json_get_str(in, "target_sha", ""),
                                        J("unmoved"));
    } else if (!strcmp(func, "verify_head_after_pull")) {
        ret = uc_verify_head_after_pull(sp, J("git_cmd"), json_get_str(in, "branch", ""),
                                        J("pre_pull_sha"), json_get_bool(in, "in_place_update", false),
                                        J("resume_token"));
    } else if (!strcmp(func, "map_ssl_cert_file_for_git")) {
        ret = uc_map_ssl_cert_file_for_git(sp, J("git_cmd"));
    } else if (!strcmp(func, "heal_stale_shallow_checkout")) {
        ret = uc_heal_stale_shallow_checkout(sp, json_get_str(in, "repo_root", ""),
                                             json_get_str(in, "branch", ""));
    } else if (!strcmp(func, "source_update_channel")) {
        ret = uc_source_update_channel(sp, J("args"), J("channel"),
                                       json_get_bool(in, "branch_explicit", false));
    } else if (!strcmp(func, "update_run_channel")) {
        ret = uc_update_run_channel(sp, J("args"));
    } else if (!strcmp(func, "settle_windows_resume")) {
        workstate = J("request") ? json_copy(J("request")) : NULL;
        ret = uc_settle_windows_resume(sp, workstate);
        echo_state = true;
    } else if (!strcmp(func, "resume_paused_gateways_at_exit")) {
        ret = uc_resume_paused_gateways_at_exit(sp, J("token"), J("request"));
    } else if (!strcmp(func, "pause_gateways_for_update")) {
        ret = uc_pause_gateways_for_update(sp, J("opts"));
    } else if (!strcmp(func, "upstream_broken_header")) {
        ret = uc_upstream_broken_header(J("broken"));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    ret = uc_norm_raise_ret(ret);
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    if (echo_state) json_set(out, "state", workstate ? json_copy(workstate) : json_object());
    uc_trace_free(&trace);
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
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    free(line);
    return 0;
}