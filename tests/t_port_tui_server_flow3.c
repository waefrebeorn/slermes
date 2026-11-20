/*
 * t_port_tui_server_flow3.c — oracle harness for the tui_gateway/server.py
 * third-wave FLOW cores / pure leaves ported in src/gateway/tui_server_flow3.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * "in" carries the injected state mirror ("state"), the scripted seam results
 * ("io") and the call arguments. The universal seam stub records one
 * {ev,args,ret} event per call in call order and returns the script (NULL ==
 * None; {"__raise__":kind,"message":m} == raised exception). A missing or
 * exhausted script is FAIL-CLOSED: the stub records and returns a RuntimeError
 * spec, matching the oracle's Script.next. The harness emits
 * {"func","in","out":{"ret","trace","state"}} per case; the oracle drives the
 * LIVE Python identically.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/tui_server_flow3.c"

typedef struct {
    json_t *io;       /* borrowed in["io"] */
    json_t *state;    /* owned mirror */
    json_t *trace;    /* owned array */
    json_t *pos;      /* owned per-name queue positions */
} ts3_script_ctx;

/* Script lookup: bare array = per-call queue; {"__script__":true,"ret":v} =
 * constant list/object; anything else = constant. Absent/exhausted sets
 * failclosed and fills fcmsg (the stub then returns a RuntimeError spec). */
static const json_t *ts3_next_script(ts3_script_ctx *c, const char *name,
                                     bool *failclosed, char *fcmsg, size_t fcsz) {
    *failclosed = false;
    const json_t *v = c->io ? json_obj_get(c->io, name) : NULL;
    if (!v) {
        *failclosed = true;
        snprintf(fcmsg, fcsz, "no script for %s", name);
        return NULL;
    }
    if (v->type == JSON_ARRAY) {
        double idx = json_get_num(c->pos, name, 0);
        size_t i = (size_t)idx;
        if (i >= json_len(v)) {
            *failclosed = true;
            snprintf(fcmsg, fcsz, "script exhausted for %s", name);
            return NULL;
        }
        json_set(c->pos, name, json_number((double)(i + 1)));
        return json_get(v, i);
    }
    if (v->type == JSON_OBJECT && json_get_bool(v, "__script__", false))
        return json_obj_get(v, "ret");
    return v;
}

static json_t *ts3_stub_call(void *ud, const char *name, const json_t *args) {
    ts3_script_ctx *c = ud;
    /* log seam: flat {ev,level,fmt,args} event (matches the oracle RecLogger). */
    if (strcmp(name, "log") == 0) {
        json_t *ev = json_object();
        json_set(ev, "ev", json_string("log"));
        json_set(ev, "level", json_copy(json_obj_get(args, "level")));
        json_set(ev, "fmt", json_copy(json_obj_get(args, "fmt")));
        json_set(ev, "args", json_copy(json_obj_get(args, "args")));
        json_append(c->trace, ev);
        return NULL;
    }
    bool fc = false;
    char fcmsg[192];
    const json_t *script = ts3_next_script(c, name, &fc, fcmsg, sizeof(fcmsg));
    json_t *ret_node;
    if (fc) ret_node = ts3_raise_spec("RuntimeError", fcmsg);
    else ret_node = script ? json_copy(script) : json_null();
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_null());
    json_set(ev, "ret", json_copy(ret_node));
    json_append(c->trace, ev);
    if (ret_node->type == JSON_NULL) { json_free(ret_node); return NULL; }
    return ret_node;
}

typedef json_t *(*ts3_entry_fn)(ts3_env *, const json_t *);
typedef struct { const char *name; ts3_entry_fn fn; } ts3_dispatch;

static const ts3_dispatch ts3_table[] = {
    {"_DropTransport.write",                ts3_drop_write},
    {"_DropTransport.close",                ts3_drop_close},
    {"register_live_transport",             ts3_register_live_transport},
    {"unregister_live_transport",           ts3_unregister_live_transport},
    {"_approval_request_payload",           ts3_approval_request_payload},
    {"_open_requests",                      ts3_open_requests},
    {"register_method",                     ts3_register_method},
    {"_enable_gateway_prompts",             ts3_enable_gateway_prompts},
    {"_clear_pending",                      ts3_clear_pending},
    {"_probe_credentials",                  ts3_probe_credentials},
    {"_probe_config_health",                ts3_probe_config_health},
    {"_tool_ctx",                           ts3_tool_ctx},
};

static void ts3_run_case(const char *func, const json_t *in, json_t *out) {
    json_t *state = json_copy(json_obj_get(in, "state"));
    if (!ts3_is_obj(state)) { json_free(state); state = json_object(); }
    if (!ts3_is_obj(json_obj_get(state, "sessions"))) json_set(state, "sessions", json_object());
    if (!ts3_is_arr(json_obj_get(state, "live_transports"))) json_set(state, "live_transports", json_array());
    if (!ts3_is_obj(json_obj_get(state, "methods"))) json_set(state, "methods", json_object());
    if (!ts3_is_arr(json_obj_get(state, "served_profile_homes"))) json_set(state, "served_profile_homes", json_array());

    ts3_script_ctx ctx = {
        .io = json_obj_get(in, "io"), .state = state,
        .trace = json_array(), .pos = json_object(),
    };
    ts3_env env = { .state = state, .call = ts3_stub_call, .ud = &ctx, .exc = NULL };
    json_t *ret = NULL;
    bool found = false;
    for (size_t i = 0; i < sizeof(ts3_table) / sizeof(ts3_table[0]); i++) {
        if (strcmp(func, ts3_table[i].name) == 0) {
            ret = ts3_table[i].fn(&env, in);
            found = true;
            break;
        }
    }
    if (!found) json_set(out, "error", json_string("unknown func"));
    if (env.exc) { json_free(ret); ret = env.exc; }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", json_copy(state));
    json_free(state);
    json_free(ctx.pos);
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
        ts3_run_case(func, in ? in : empty, out);
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
