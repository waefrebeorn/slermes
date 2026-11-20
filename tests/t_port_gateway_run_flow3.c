/*
 * t_port_gateway_run_flow3.c — oracle harness for the gateway/run.py FLOW
 * cores + leaves ported in src/gateway/gateway_run_flow3.c (third wave).
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads
 * JSON-line cases from stdin (tests/fixtures/gateway_run_flow3.jsonl), wires
 * the universal seam to the SCRIPTED results in each case's in.io object (a
 * recording stub appending one trace event per seam call, in call order),
 * and emits {"func":..., "in":..., "out":{"ret":..., "trace":[...],
 * "state":null}}.
 *
 * Script shapes: a bare JSON array is a SEQUENCE (one result per call, the
 * LAST element repeats after exhaustion); any other value is reused on every
 * call; a scripted JSON null is delivered as a NULL pointer (Python None).
 * Fail-closed: a MISSING or EMPTY script yields a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret then propagated or
 * swallowed by the port exactly as live. BaseException kinds pass through
 * every except Exception site.
 *
 * tests/oracle_gateway_run_flow3.py drives the LIVE Python with the same
 * scripts and compares the ordered seam-call trace plus the return value.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/gateway_run_flow3.c"

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

/* Platform enum value membership (gateway/config.py Platform). */
static const char *gr3_platform_values[] = {
    "local", "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
    "signal", "mattermost", "matrix", "email", "sms", "dingtalk", "api_server",
    "webhook", "msgraph_webhook", "feishu", "wecom", "wecom_callback", "weixin",
    "bluebubbles", "qqbot", "relay"};

static bool valid_platform(const char *s) {
    if (!s) return false;
    for (size_t i = 0; i < sizeof(gr3_platform_values)/sizeof(gr3_platform_values[0]); i++)
        if (strcmp(s, gr3_platform_values[i]) == 0) return true;
    return false;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    gr3_seams_t seams = { &ctx, script_call };
    const gr3_seams_t *sp = &seams;
    json_t *r = NULL;

    if (strcmp(func, "telegramize_command_mentions") == 0) {
        r = gr3_telegramize_command_mentions(J("platform"), S("text"),
                                             B("import_fail", false), sp);
    } else if (strcmp(func, "watch_gateway_turn_inactivity") == 0) {
        r = gr3_watch_gateway_turn_inactivity(J("agent_holder"), S("task_id"),
                J("process_baseline"), N("timeout", 0), S("worker_done"),
                S("timeout_fired"), S("cleanup_lock"), N("poll_interval", 5.0),
                S("is_still_current"), sp);
    } else if (strcmp(func, "builtin_adapter_import") == 0) {
        r = gr3_builtin_adapter_import(S("module"), S("adapter_name"),
                                       S("requirement"), sp);
    } else if (strcmp(func, "instantiate_builtin_adapter") == 0) {
        /* materialize the Platform value BEFORE dispatch (live drive does
         * the same; an invalid value raises with an empty trace) */
        const char *pv = S("platform");
        if (!valid_platform(pv)) {
            char m[160];
            snprintf(m, sizeof(m), "'%s' is not a valid Platform", pv ? pv : "");
            r = make_raise("ValueError", m);
        } else {
            r = gr3_instantiate_builtin_adapter(pv, J("config"), sp);
        }
    } else if (strcmp(func, "housekeeping_channel_directory") == 0) {
        r = gr3_housekeeping_channel_directory(J("adapters"), S("loop"), sp);
    } else if (strcmp(func, "housekeeping_curator") == 0) {
        r = gr3_housekeeping_curator(J("idle_fence"), J("summary_msgs"), sp);
    } else if (strcmp(func, "housekeeping_state_db_maintenance") == 0) {
        r = gr3_housekeeping_state_db_maintenance(J("launch"), sp);
    } else if (strcmp(func, "housekeeping_deferred_fts_retry") == 0) {
        r = gr3_housekeeping_deferred_fts_retry(sp);
    } else if (strcmp(func, "start_gateway_make_restart_signal_handler") == 0) {
        r = gr3_start_gateway_make_restart_signal_handler(J("runner"), sp);
    } else if (strcmp(func, "running_agent_ids") == 0) {
        r = gr3_running_agent_ids(J("runner"), sp);
    } else if (strcmp(func, "admit_bot_message_for_source") == 0) {
        r = gr3_admit_bot_message_for_source(J("runner"), J("source"), sp);
    } else if (strcmp(func, "under_authorization_profile") == 0) {
        r = gr3_under_authorization_profile(J("runner"), J("source"), sp);
    } else if (strcmp(func, "session_env_scope") == 0) {
        r = gr3_session_env_scope(J("runner"), J("context"), sp);
    } else if (strcmp(func, "run_housekeeping_in_executor") == 0) {
        r = gr3_run_housekeeping_in_executor(J("runner"), S("func"), J("args"), sp);
    } else if (strcmp(func, "get_housekeeping_executor") == 0) {
        r = gr3_get_housekeeping_executor(J("runner"), sp);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", json_null());
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
