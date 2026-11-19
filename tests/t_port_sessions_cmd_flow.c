/* t_port_sessions_cmd_flow.c — harness for the hermes_cli/sessions_cmd.py
 * session-management FLOW port (src/cli/port_sessions_cmd_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}} where "in"
 * carries the call arguments plus "env" (profile_dir) and "io" (seam-name ->
 * script: single value, {"$seq": [...]} per-call queue, or
 * {"__raise__": cls, "msg": m, ...}). Emits JSON LINES:
 *   {"func", "in", "out": {"ret", "trace", "state"}}
 * "ret" is the canonical return (tuples/sets -> arrays, uncaught exception ->
 * {"__raise__": cls}); "trace" is the ordered seam-call sequence
 * [{"ev","args","ret"}]; "state" is the post-call vars(args) window for
 * _export_flat ({} otherwise, partial window even on a raise). The oracle
 * drives LIVE hermes_cli/sessions_cmd.py under the SAME scripts and diffs out.
 */
#include "hermes_json.h"
#include "port_sessions_cmd_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A string field with an explicit-null distinction: absent -> def, present
 * null -> NULL (None), present string -> its value. */
static const char *scf_harness_str(json_t *in, const char *key, const char *def) {
    json_t *v = json_obj_get(in, key);
    if (!v) return def;
    if (v->type == JSON_STRING) return v->str_val;
    return NULL;
}

static json_t *scf_harness_run(scf_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_any_filter_args") == 0)
        return scf_any_filter_args(ctx, json_obj_get(in, "args"));
    if (strcmp(func, "_prune_args") == 0)
        return scf_prune_args(ctx, json_obj_get(in, "filters"));
    if (strcmp(func, "_confirm_or_exit_code") == 0)
        return scf_confirm_or_exit_code(ctx, scf_harness_str(in, "prompt", ""),
                                        scf_harness_str(in, "declined", "Cancelled."),
                                        scf_harness_str(in, "unattended", "--yes"));
    if (strcmp(func, "_not_found") == 0)
        return scf_not_found(ctx, json_obj_get(in, "session_id"));
    if (strcmp(func, "_print_empty_store") == 0)
        return scf_print_empty_store(ctx, json_get_str(in, "action", ""),
                                     json_obj_get(in, "args"));
    if (strcmp(func, "_delete") == 0)
        return scf_delete(ctx, json_get_str(in, "session_id", ""),
                          json_obj_get(in, "fences"));
    if (strcmp(func, "_cmd_delete") == 0)
        return scf_cmd_delete(ctx, json_obj_get(in, "args"));
    if (strcmp(func, "_cmd_rename") == 0)
        return scf_cmd_rename(ctx, json_obj_get(in, "args"));
    if (strcmp(func, "_cmd_pin") == 0)
        return scf_cmd_pin(ctx, json_obj_get(in, "args"),
                           json_get_bool(in, "pinning", false));
    if (strcmp(func, "_print_dry_run_preview") == 0) {
        json_t *cands = json_obj_get(in, "candidates");
        json_t *empty = NULL;
        if (!cands) { empty = json_array(); cands = empty; }
        json_t *ret = scf_print_dry_run_preview(ctx, cands, json_obj_get(in, "filters"));
        json_free(empty);
        return ret;
    }
    if (strcmp(func, "_note_pinned_skipped") == 0)
        return scf_note_pinned_skipped(ctx, json_obj_get(in, "filters"),
                                       json_get_str(in, "action", ""));
    if (strcmp(func, "_output_file_in_dir") == 0)
        return scf_output_file_in_dir(ctx, json_obj_get(in, "output"),
                                      json_get_str(in, "default_name", ""));
    if (strcmp(func, "_write_output") == 0)
        return scf_write_output(ctx, json_obj_get(in, "output"),
                                json_get_str(in, "text", ""),
                                json_get_str(in, "summary", ""));
    if (strcmp(func, "_render_only") == 0)
        return scf_render_only(ctx, json_obj_get(in, "args"), json_obj_get(in, "sessions"));
    if (strcmp(func, "_render_html") == 0)
        return scf_render_html(ctx, json_obj_get(in, "args"), json_obj_get(in, "sessions"));
    if (strcmp(func, "_export_flat") == 0) {
        json_t *args = json_copy(json_obj_get(in, "args"));
        if (!args) args = json_object();
        json_t *ret = scf_export_flat(ctx, json_get_str(in, "kind", ""), args);
        *state_out = args; /* emitted even when an exception propagated */
        return ret;
    }
    if (strcmp(func, "_print_recovery_verdict") == 0)
        return scf_print_recovery_verdict(ctx, json_obj_get(in, "report"),
                                          json_obj_get(in, "output"),
                                          json_get_bool(in, "allow_partial", false));
    if (strcmp(func, "_repair_prompts_pin_names") == 0) {
        json_t *record = json_obj_get(in, "record");
        json_t *empty = NULL;
        if (!record) { empty = json_object(); record = empty; }
        json_t *ret = scf_repair_prompts_pin_names(ctx, record);
        json_free(empty);
        return ret;
    }
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 20];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) {
            fprintf(stderr, "harness: unparseable case line\n");
            return 1;
        }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *empty_in = NULL;
        if (!in) { empty_in = json_object(); in = empty_in; }
        scf_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = scf_harness_run(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_copy(json_obj_get(ctx.exc, "__raise__")));
            json_free(ctx.exc);
            json_free(ret);
        } else {
            ret_out = ret ? ret : json_null();
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", state ? state : json_object());
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) {
            fputs(s, stdout);
            fputc('\n', stdout);
            free(s);
        }
        json_free(emit);
        json_free(ctx.counters);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}
