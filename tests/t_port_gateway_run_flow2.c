/*
 * t_port_gateway_run_flow2.c — oracle harness for the gateway/run.py FLOW
 * cores + leaves ported in src/gateway/gateway_run_flow2.c (second wave).
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads
 * JSON-line cases from stdin (tests/fixtures/gateway_run_flow2.jsonl), wires
 * the universal seam to the SCRIPTED results in each case's in.io object (a
 * recording stub appending one trace event per seam call, in call order),
 * and emits {"func":..., "in":..., "out":{"ret":..., "trace":[...],
 * "state":...}}. The env model (in.env) is mutated in place by the bridge
 * family and echoed back as out.state.env.
 *
 * Script shapes: a bare JSON array is a SEQUENCE (one result per call, the
 * LAST element repeats after exhaustion); any other value is reused on every
 * call; a scripted JSON null is delivered as a NULL pointer (Python None).
 * Fail-closed: a MISSING or EMPTY script yields a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret then propagated or
 * swallowed by the port exactly as live. BaseException kinds pass through
 * every except Exception site.
 *
 * tests/oracle_gateway_run_flow2.py drives the LIVE Python with the same
 * scripts and compares the ordered decision sequence + return + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/gateway_run_flow2.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static json_t *args_drop_null(const json_t *args) {
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

static json_t *script_call(void *vctx, const char *name, const json_t *args) {
    script_ctx_t *ctx = vctx;
    const json_t *io = json_obj_get(ctx->in, "io");
    const json_t *sc = io ? json_obj_get(io, name) : NULL;
    json_t *result = NULL;
    bool fail_closed = false;
    if (sc && sc->type == JSON_ARRAY) {
        size_t n = json_len(sc);
        if (n == 0) {
            fail_closed = true;
        } else {
            double idx = json_get_num(ctx->counters, name, 0);
            size_t i = (size_t)idx;
            result = json_copy(json_get(sc, i < n ? i : n - 1));
            json_set(ctx->counters, name, json_number(idx + 1));
        }
    } else if (sc) {
        result = json_copy(sc);
    } else {
        fail_closed = true;
    }
    if (fail_closed) {
        char m[160];
        snprintf(m, sizeof(m), "no script for %s", name);
        result = make_raise("RuntimeError", m);
    }
    /* A scripted JSON null is delivered as a NULL pointer, not a node. */
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    json_append(ctx->trace, e);
    return result;
}

#define J(k) json_obj_get(in, k)
#define S(k) json_get_str(in, k, NULL)
#define N(k, d) json_get_num(in, k, d)
#define B(k, d) json_get_bool(in, k, d)

static const char *gr2_message_type_values[] = {
    "text", "location", "photo", "video", "audio", "voice", "document",
    "sticker", "command"};

static bool valid_message_type(const char *s) {
    if (!s) return false;
    for (size_t i = 0; i < sizeof(gr2_message_type_values) / sizeof(gr2_message_type_values[0]); i++)
        if (strcmp(s, gr2_message_type_values[i]) == 0) return true;
    return false;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    gr2_seams_t seams = { &ctx, script_call };
    const gr2_seams_t *sp = &seams;
    json_t *r = NULL;
    json_t *state = NULL;
    json_t *env = NULL;

    /* bridge family: env is mutable state echoed back as out.state.env */
    bool bridge = strcmp(func, "bridge_section_to_env") == 0
               || strcmp(func, "bridge_max_turns_to_env") == 0
               || strcmp(func, "bridge_terminal_config_to_env") == 0
               || strcmp(func, "bridge_auxiliary_config_to_env") == 0
               || strcmp(func, "bridge_config_to_env") == 0;
    if (bridge) {
        const json_t *e = J("env");
        env = (e && e->type == JSON_OBJECT) ? json_copy(e) : json_object();
    }

    if (strcmp(func, "context_overflow_reply") == 0) {
        r = gr2_context_overflow_reply(sp);
    } else if (strcmp(func, "rate_limit_reply") == 0) {
        r = gr2_rate_limit_reply(S("text"), sp);
    } else if (strcmp(func, "bridge_section_to_env") == 0) {
        r = gr2_bridge_section_to_env(J("section"), J("mapping"), env);
    } else if (strcmp(func, "bridge_max_turns_to_env") == 0) {
        r = gr2_bridge_max_turns_to_env(J("agent_cfg"), env);
    } else if (strcmp(func, "bridge_terminal_config_to_env") == 0) {
        r = gr2_bridge_terminal_config_to_env(J("terminal_cfg"), env, sp);
    } else if (strcmp(func, "bridge_auxiliary_config_to_env") == 0) {
        r = gr2_bridge_auxiliary_config_to_env(J("auxiliary_cfg"), env, sp);
    } else if (strcmp(func, "bridge_config_to_env") == 0) {
        r = gr2_bridge_config_to_env(J("cfg"), env, sp);
    } else if (strcmp(func, "load_bridge_config") == 0) {
        r = gr2_load_bridge_config(S("config_path"), sp);
    } else if (strcmp(func, "best_effort") == 0) {
        r = gr2_best_effort(S("debug_msg"), sp);
    } else if (strcmp(func, "runtime_agent_kwargs") == 0) {
        r = gr2_runtime_agent_kwargs(J("runtime"));
    } else if (strcmp(func, "event_media_kind_is") == 0) {
        /* materialize MessageType values BEFORE dispatch (live drive does
         * the same; an invalid value raises with an empty trace) */
        const json_t *event = J("event");
        const json_t *mt = gr2_oget(event, "message_type");
        const char *mts = gr2_sval(mt);
        const json_t *fb = J("fallback_types");
        char m[160];
        if (mt && mt->type != JSON_NULL && !valid_message_type(mts)) {
            snprintf(m, sizeof(m), "'%s' is not a valid MessageType", mts ? mts : "");
            r = make_raise("ValueError", m);
        } else {
            bool bad = false;
            if (fb && fb->type == JSON_ARRAY) {
                for (size_t i = 0; i < json_len(fb); i++) {
                    const char *f = gr2_sval(json_get(fb, i));
                    if (!valid_message_type(f)) {
                        snprintf(m, sizeof(m), "'%s' is not a valid MessageType", f ? f : "");
                        r = make_raise("ValueError", m);
                        bad = true;
                        break;
                    }
                }
            }
            if (!bad)
                r = gr2_event_media_kind_is(event, N("index", 0), S("mime_prefix"), fb, sp);
        }
    } else if (strcmp(func, "format_gateway_process_notification") == 0) {
        r = gr2_format_gateway_process_notification(J("evt"), sp);
    } else if (strcmp(func, "reconnect_attention_after_secs") == 0) {
        r = gr2_reconnect_attention_after_secs(sp);
    } else if (strcmp(func, "write_runtime_status_quiet") == 0) {
        r = gr2_write_runtime_status_quiet(J("fields"), sp);
    } else if (strcmp(func, "clear_takeover_marker_quiet") == 0) {
        r = gr2_clear_takeover_marker_quiet(sp);
    } else if (strcmp(func, "housekeeping_chore") == 0) {
        r = gr2_housekeeping_chore(S("label"), J("args"), J("kwargs"), sp);
    } else if (strcmp(func, "housekeeping_paste_sweep") == 0) {
        r = gr2_housekeeping_paste_sweep(sp);
    } else if (strcmp(func, "housekeeping_misfire_catch_up") == 0) {
        r = gr2_housekeeping_misfire_catch_up(J("cron_provider"), J("adapters"), J("loop"), sp);
    } else if (strcmp(func, "housekeeping_memory_trim") == 0) {
        r = gr2_housekeeping_memory_trim(sp);
    } else if (strcmp(func, "housekeeping_checkpoint_prune") == 0) {
        r = gr2_housekeeping_checkpoint_prune(sp);
    } else if (strcmp(func, "housekeeping_plugin_update_check") == 0) {
        r = gr2_housekeeping_plugin_update_check(sp);
    } else if (strcmp(func, "launch_sessions_dir") == 0) {
        r = gr2_launch_sessions_dir(J("config"), sp);
    } else if (strcmp(func, "profile_sessions_dir") == 0) {
        r = gr2_profile_sessions_dir(J("launch"), sp);
    } else if (strcmp(func, "guard_corrupt_user_config") == 0) {
        r = gr2_guard_corrupt_user_config(sp);
    } else if (strcmp(func, "wait_for_pid_exit") == 0) {
        r = gr2_wait_for_pid_exit(N("pid", 0), N("attempts", 0), N("delay", 0), sp);
    } else if (strcmp(func, "resolve_handoff_watch_scopes") == 0) {
        r = gr2_resolve_handoff_watch_scopes(J("runner"), sp);
    } else if (strcmp(func, "log_standalone_profiles_at_boot") == 0) {
        r = gr2_log_standalone_profiles_at_boot(J("runner"), sp);
    } else if (strcmp(func, "host_attach_or_none") == 0) {
        r = gr2_host_attach_or_none(B("replace", false), B("force", false), sp);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    if (bridge) {
        state = json_object();
        json_set(state, "env", env);
    }
    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", state ? state : json_null());
    json_free(ctx.counters);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
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
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}