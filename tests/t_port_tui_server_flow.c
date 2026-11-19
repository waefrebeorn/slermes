/*
 * t_port_tui_server_flow.c — oracle harness for the tui_gateway/server.py
 * FLOW cores + leaves ported in src/gateway/tui_server_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * "in" carries the injected state mirror ("state"), the scripted seam results
 * ("io") and the call arguments. The universal seam stub records one
 * {ev, args, ret} trace event per call in call order and returns the script
 * (NULL == None; a {"__raise__":...} spec is returned for the port to raise).
 * A missing/exhausted script is FAIL-CLOSED: the stub records and returns a
 * {"__raise__":"RuntimeError","message":"no script for X"} spec, matching the
 * oracle's Script.next. The harness emits {"func","in","out":{ret,trace,state}}
 * per case; tests/oracle_tui_server_flow.py drives the LIVE Python identically.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/tui_server_flow.c"

typedef struct {
    const json_t *in;
    json_t *io;       /* borrowed in["io"] */
    json_t *state;    /* owned mirror */
    json_t *trace;    /* owned array */
    json_t *pos;      /* owned per-name queue positions */
} ts_script_ctx;

/* Script lookup: bare array = per-call queue; {"__script__":true,"ret":v} =
 * constant list/object; anything else = constant. Absent/exhausted sets
 * *failclosed and fills fcmsg (the stub then returns a RuntimeError spec). */
static const json_t *ts_next_script(ts_script_ctx *c, const char *name,
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

static json_t *ts_stub_call(void *ud, const char *name, const json_t *args) {
    ts_script_ctx *c = ud;
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
    const json_t *script = ts_next_script(c, name, &fc, fcmsg, sizeof(fcmsg));
    json_t *ret_node;
    if (fc) ret_node = ts_raise_spec("RuntimeError", fcmsg);   /* owned */
    else ret_node = script ? json_copy(script) : json_null();
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_null());
    json_set(ev, "ret", json_copy(ret_node));
    json_append(c->trace, ev);
    if (ret_node->type == JSON_NULL) { json_free(ret_node); return NULL; }
    return ret_node;   /* owned; the port frees it (ts_call result) */
}

typedef json_t *(*ts_entry_fn)(ts_env *, const json_t *);
typedef struct { const char *name; ts_entry_fn fn; } ts_dispatch;

static const ts_dispatch ts_table[] = {
    {"_ok", ts_ok},
    {"_normalize_request", ts_normalize_request},
    {"_coerce_int_config_value", ts_coerce_int_config_value},
    {"_display_cfg", ts_display_cfg},
    {"_load_interim_assistant_messages", ts_load_interim_assistant_messages},
    {"_load_dashboard_process_isolation_config", ts_load_dashboard_process_isolation_config},
    {"_active_config_path", ts_active_config_path},
    {"_load_cfg", ts_load_cfg},
    {"_drop_cfg_cache", ts_drop_cfg_cache},
    {"_load_cfg_raw", ts_load_cfg_raw},
    {"_save_cfg", ts_save_cfg},
    {"_write_config_changes", ts_write_config_changes},
    {"_write_config_key", ts_write_config_key},
    {"_write_config_keys", ts_write_config_keys},
    {"_is_model_switch_marker", ts_is_model_switch_marker},
    {"_is_pivot_marker", ts_is_pivot_marker},
    {"_turn_started_at", ts_turn_started_at},
    {"_cli_exec_blocked", ts_cli_exec_blocked},
    {"_clarify_timeout_seconds", ts_clarify_timeout_seconds},
    {"_ask", ts_ask},
    {"_clarify_block", ts_clarify_block},
    {"_tour_request", ts_tour_request},
    {"_preview_action_request", ts_preview_action_request},
    {"_status_update", ts_status_update},
    {"_resolve_name", ts_resolve_name},
    {"_finish_reload", ts_finish_reload},
};

static void ts_ensure_obj(json_t *o, const char *k) {
    if (!json_has(o, k)) json_set(o, k, json_null());
}

static void ts_run_case(const char *func, const json_t *in, json_t *out) {
    json_t *state = json_copy(json_obj_get(in, "state"));
    if (!ts_is_obj(state)) { json_free(state); state = json_object(); }
    static const char *sects[] = {"cfg", "home", "sessions", "reprobe", "mcp"};
    for (int i = 0; i < 5; i++)
        if (!json_obj_get(state, sects[i])) json_set(state, sects[i], json_object());
    json_t *cfg = json_obj_get(state, "cfg");
    ts_ensure_obj(cfg, "cache"); ts_ensure_obj(cfg, "sig"); ts_ensure_obj(cfg, "path");
    json_t *home = json_obj_get(state, "home");
    if (!json_has(home, "hermes_home")) json_set(home, "hermes_home", json_string(""));
    if (!json_has(home, "at_import")) json_set(home, "at_import", json_string(""));
    json_t *mcp = json_obj_get(state, "mcp");
    if (!json_has(mcp, "loaded_rev")) json_set(mcp, "loaded_rev", json_string(""));

    ts_script_ctx ctx = {
        .in = in, .io = json_obj_get(in, "io"), .state = state,
        .trace = json_array(), .pos = json_object(),
    };
    ts_env env = { .state = state, .call = ts_stub_call, .ud = &ctx, .exc = NULL };
    json_t *ret = NULL;
    bool found = false;
    for (size_t i = 0; i < sizeof(ts_table) / sizeof(ts_table[0]); i++) {
        if (strcmp(func, ts_table[i].name) == 0) {
            ret = ts_table[i].fn(&env, in);
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
        ts_run_case(func, in ? in : empty, out);
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