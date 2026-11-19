/* t_port_main_cli_flow2.c — harness for the hermes_cli/main.py UNCLAIMED-remainder
 * FLOW port (src/cli/main_cli_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}, "env": {...}}}. Emits JSON LINES: {"func","in","out":{"ret","trace",
 * "state"}}. "ret" is the canonical return (tuples/sets -> arrays, None -> null,
 * uncaught exception -> {"__raise__": cls}); "trace" is the ordered seam-call
 * sequence [{"ev","args","ret"}]; "state" is the post-call out-state (mutated
 * args for _resolve_chat_session_args / _promote_top_level_resume; {} otherwise,
 * and {} on any raise). The oracle drives LIVE hermes_cli/main.py under the SAME
 * scripts and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "cli/main_cli_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *mc2_harness_run(mc2_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_warn_if_unsupervised_pid1") == 0)
        return mc2_warn_if_unsupervised_pid1(ctx, in);
    if (strcmp(func, "_read_git_revision_fingerprint") == 0)
        return mc2_read_git_revision_fingerprint(ctx, in);
    if (strcmp(func, "_auth_store_logged_in") == 0)
        return mc2_auth_store_logged_in(ctx, in);
    if (strcmp(func, "_resolve_workspace_key") == 0)
        return mc2_resolve_workspace_key(ctx, in);
    if (strcmp(func, "_session_db") == 0)
        return mc2_session_db(ctx, in, mc2_body_scripted, in);
    if (strcmp(func, "_resolve_chat_session_args") == 0) {
        json_t *args = NULL;
        json_t *r = mc2_resolve_chat_session_args(ctx, in, &args);
        if (!mc2_exc_pending(ctx) && args) {
            json_t *st = json_object();
            json_set(st, "args", args);  /* ownership transferred */
            *state_out = st;
        } else {
            json_free(args);
        }
        return r;
    }
    if (strcmp(func, "_warn_retired_xai_models") == 0)
        return mc2_warn_retired_xai_models(ctx, in);
    if (strcmp(func, "_start_chat_background_prefetch") == 0)
        return mc2_start_chat_background_prefetch(ctx, in);
    if (strcmp(func, "_forward_command") == 0)
        return mc2_forward_command(ctx, in);
    if (strcmp(func, "_resolve_active_provider") == 0)
        return mc2_resolve_active_provider(ctx, in);
    if (strcmp(func, "_pick_provider") == 0)
        return mc2_pick_provider(ctx, in);
    if (strcmp(func, "_dashboard_prepare_runtime") == 0)
        return mc2_dashboard_prepare_runtime(ctx, in);
    if (strcmp(func, "_first_positional_argv") == 0)
        return mc2_first_positional_argv(ctx, in);
    if (strcmp(func, "_resolve_deferred_platform_cli_command") == 0)
        return mc2_resolve_deferred_platform_cli_command(ctx, json_get_str(in, "command_name", ""));
    if (strcmp(func, "_agent_subcommand_selected") == 0)
        return mc2_agent_subcommand_selected(ctx, in);
    if (strcmp(func, "_chat_runs_at_gateway") == 0)
        return mc2_chat_runs_at_gateway(ctx, in);
    if (strcmp(func, "_apply_user_config_bypass") == 0)
        return mc2_apply_user_config_bypass(ctx, in);
    if (strcmp(func, "_run_oneshot_from_args") == 0)
        return mc2_run_oneshot_from_args(ctx, in);
    if (strcmp(func, "_enable_cpu_profile") == 0)
        return mc2_enable_cpu_profile(ctx, in);
    if (strcmp(func, "_light_chat_parser") == 0)
        return mc2_light_chat_parser(ctx, in);
    if (strcmp(func, "_promote_top_level_resume") == 0) {
        json_t *args = NULL;
        json_t *r = mc2_promote_top_level_resume(ctx, in, &args);
        if (!mc2_exc_pending(ctx) && args) {
            json_t *st = json_object();
            json_set(st, "args", args);
            *state_out = st;
        } else {
            json_free(args);
        }
        return r;
    }
    if (strcmp(func, "_try_fast_serve_launch") == 0)
        return mc2_try_fast_serve_launch(ctx, in);
    if (strcmp(func, "_attach_plugin_cli_command") == 0)
        return mc2_attach_plugin_cli_command(ctx, mc2_get(in, "cmd_info"));
    if (strcmp(func, "_register_plugin_cli_commands") == 0)
        return mc2_register_plugin_cli_commands(ctx, in);
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 16];
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
        mc2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = mc2_harness_run(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(mc2_exc_class(&ctx)));
            json_free(ctx.exc);
            json_free(ret);
            json_free(state);
            state = NULL;
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