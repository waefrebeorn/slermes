/*
 * t_port_hosted_room_driver_flow2.c — oracle harness for the UNCLAIMED
 * stop/cancel/recovery remainder of tui_gateway/hosted_room_driver.py ported in
 * src/gateway/hosted_room_driver_flow2.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * "in" carries the injected runtime-state mirror ("state"), the scripted seam
 * results ("io") and the call arguments. The universal seam stub records one
 * {ev, args, ret} trace event per call in call order and returns the script
 * (NULL == None; a {"__raise__":...} spec is returned for the port to raise).
 * A missing/exhausted script is FAIL-CLOSED: the stub records and returns a
 * {"__raise__":"RuntimeError","message":"no script for X"} spec, matching the
 * oracle's next_script. The harness emits {"func","in","out":{ret,trace,state}}
 * per case; tests/oracle_hosted_room_driver_flow2.py drives LIVE Python identically.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/hosted_room_driver_flow2.c"

typedef struct {
    const json_t *in;
    json_t *io;       /* borrowed in["io"] */
    json_t *trace;    /* owned array */
    json_t *pos;      /* owned per-name queue positions */
} hrd2_script_ctx;

/* Script lookup: bare array = per-call queue; {"__script__":true,"ret":v} =
 * constant list/object; anything else = constant. Absent/exhausted sets
 * *failclosed and fills fcmsg (the stub then returns a RuntimeError spec). */
static const json_t *hrd2_next_script(hrd2_script_ctx *c, const char *name,
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

static json_t *hrd2_stub_call(void *ud, const char *name, const json_t *args) {
    hrd2_script_ctx *c = ud;
    bool fc = false;
    char fcmsg[192];
    const json_t *script = hrd2_next_script(c, name, &fc, fcmsg, sizeof(fcmsg));
    json_t *ret_node;
    if (fc) ret_node = hrd2_raise_spec("RuntimeError", fcmsg);   /* owned */
    else ret_node = script ? json_copy(script) : json_null();
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_null());
    json_set(ev, "ret", json_copy(ret_node));
    json_append(c->trace, ev);
    if (ret_node->type == JSON_NULL) { json_free(ret_node); return NULL; }
    return ret_node;   /* owned; the port frees it (hrd2_call result) */
}

/* ── state defaulting (mirrors the oracle's setup_case defaults) ─────────── */
static void hrd2_default_obj(json_t *state, const char *key) {
    if (!hrd2_is_obj(json_obj_get(state, key))) json_set(state, key, json_object());
}
static void hrd2_default_arr(json_t *state, const char *key) {
    if (!hrd2_is_arr(json_obj_get(state, key))) json_set(state, key, json_array());
}
static void hrd2_state_defaults(json_t *state) {
    hrd2_default_obj(state, "cfg");
    json_t *cfg = json_obj_get(state, "cfg");
    static const struct { const char *k; double v; } cfgd[] = {
        {"lease_ttl_seconds", 30.0}, {"poll_interval_seconds", 5.0},
        {"active_poll_interval_seconds", 0.25}, {"turn_timeout_seconds", 1830.0},
        {"indeterminate_defer_seconds", 60.0}, {"unavailable_retry_min_seconds", 1.0},
        {"unavailable_retry_max_seconds", 30.0},
    };
    for (size_t i = 0; i < sizeof(cfgd) / sizeof(cfgd[0]); i++)
        if (!json_has(cfg, cfgd[i].k)) json_set(cfg, cfgd[i].k, json_number(cfgd[i].v));
    if (!json_has(cfg, "process_generation"))
        json_set(cfg, "process_generation", json_string("pg-test"));
    if (!json_has(state, "placement")) json_set(state, "placement", json_string("shared"));
    hrd2_default_obj(state, "cb");
    json_t *cb = json_obj_get(state, "cb");
    static const char *cbs[] = {"remote_member", "confirm_deferred_stop", "publish_terminal",
        "pending_action", "task_progress", "retire_task_progress", "transport_resolver",
        "rpc", "prepare_room", "maintain"};
    for (size_t i = 0; i < sizeof(cbs) / sizeof(cbs[0]); i++)
        if (!json_has(cb, cbs[i])) json_set(cb, cbs[i], json_bool(strcmp(cbs[i], "rpc") == 0));
    if (!json_has(state, "rpc_id")) json_set(state, "rpc_id", json_string("rpc"));
    hrd2_default_arr(state, "rooms");
    hrd2_default_obj(state, "leases");
    json_t *leases = json_obj_get(state, "leases");
    for (size_t i = 0; i < leases->c.count; i++) {
        json_t *n = hrd2_lease_norm(leases->c.items[i]);
        if (n) { json_free(leases->c.items[i]); leases->c.items[i] = n; }
    }
    hrd2_default_arr(state, "blocked");
    hrd2_default_obj(state, "ambiguous");
    hrd2_default_arr(state, "route_retries");
    hrd2_default_arr(state, "recovered");
    hrd2_default_arr(state, "inspected");
    if (!json_has(state, "last_error")) json_set(state, "last_error", json_null());
    hrd2_default_arr(state, "room_threads");
    hrd2_default_arr(state, "reschedule");
    if (!json_has(state, "wake_set")) json_set(state, "wake_set", json_bool(false));
    if (!json_has(state, "stop_set")) json_set(state, "stop_set", json_bool(false));
}

/* sort a string array in place (blocked / reschedule / room_threads emit) */
static void hrd2_sort_strs(json_t *arr) {
    for (size_t i = 1; i < json_len(arr); i++) {
        json_t *key = arr->c.items[i];
        size_t j = i;
        while (j > 0 && strcmp(hrd2_sval(arr->c.items[j - 1]), hrd2_sval(key)) > 0) {
            arr->c.items[j] = arr->c.items[j - 1];
            j--;
        }
        arr->c.items[j] = key;
    }
}
static int hrd2_tuple_cmp(const json_t *a, const json_t *b) {
    size_t n = json_len(a) < json_len(b) ? json_len(a) : json_len(b);
    for (size_t i = 0; i < n; i++) {
        const json_t *x = json_get(a, i);
        const json_t *y = json_get(b, i);
        if (hrd2_is_str(x) && hrd2_is_str(y)) {
            int c = strcmp(hrd2_str(x) ? hrd2_str(x) : "", hrd2_str(y) ? hrd2_str(y) : "");
            if (c) return c;
        } else if (hrd2_is_num(x) && hrd2_is_num(y)) {
            if (x->num_val != y->num_val) return x->num_val < y->num_val ? -1 : 1;
        }
    }
    return 0;
}
static void hrd2_sort_tuples(json_t *arr) {
    for (size_t i = 1; i < json_len(arr); i++) {
        json_t *key = arr->c.items[i];
        size_t j = i;
        while (j > 0 && hrd2_tuple_cmp(arr->c.items[j - 1], key) > 0) {
            arr->c.items[j] = arr->c.items[j - 1];
            j--;
        }
        arr->c.items[j] = key;
    }
}
static void hrd2_state_emit_norm(json_t *state) {
    hrd2_sort_strs(json_obj_get(state, "blocked"));
    hrd2_sort_strs(json_obj_get(state, "reschedule"));
    hrd2_sort_strs(json_obj_get(state, "room_threads"));
    hrd2_sort_tuples(json_obj_get(state, "recovered"));
    hrd2_sort_tuples(json_obj_get(state, "inspected"));
}

static void hrd2_run_case(const char *func, const json_t *in, json_t *out) {
    json_t *state = json_copy(json_obj_get(in, "state"));
    if (!hrd2_is_obj(state)) { json_free(state); state = json_object(); }
    hrd2_state_defaults(state);
    hrd2_script_ctx ctx = {
        .in = in, .io = json_obj_get(in, "io"),
        .trace = json_array(), .pos = json_object(),
    };
    hrd2_env env = { .state = state, .call = hrd2_stub_call, .ud = &ctx };
    json_t *ret = NULL;
    bool found = false;
    for (size_t i = 0; i < sizeof(hrd2_table) / sizeof(hrd2_table[0]); i++) {
        if (strcmp(func, hrd2_table[i].name) == 0) {
            ret = hrd2_table[i].fn(&env, in);
            found = true;
            break;
        }
    }
    if (!found) json_set(out, "error", json_string("unknown func"));
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", ctx.trace);
    hrd2_state_emit_norm(state);
    json_set(out, "state", state);
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
        hrd2_run_case(func, in ? in : empty, out);
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