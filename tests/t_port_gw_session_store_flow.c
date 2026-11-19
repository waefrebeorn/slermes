/*
 * t_port_gw_session_store_flow.c — oracle harness for the
 * hermes_cli/gateway_session_store.py FLOW ports in
 * src/cli/port_gw_session_store_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires the universal seam to the SCRIPTED results in in.io (recording stubs
 * appending one trace event per seam call, in call order), seeds the store
 * state mirror from in.st, and emits {"func","in","out":{"ret","trace","st"}}
 * per case. tests/oracle_gw_session_store_flow.py drives the LIVE Python with
 * the same scripts and compares the ordered decision sequence + return + state.
 *
 * Script semantics (identical on the oracle side):
 *   io[name] absent        -> RuntimeError "no script for <name>" (fail-closed)
 *   io[name] = array       -> per-call queue (exhaustion -> RuntimeError)
 *   io[name] = {"__script__":true,"ret":X} -> constant list/object return X
 *   io[name] = value       -> constant return (deep-copied per call)
 *   {"__raise__":cls,"message":msg} -> a raised exception (KeyError msg is the
 *   RAW key; str(KeyError(k))==repr(k) is applied once at emit).
 * Structural seam "connection.state" get-or-creates the mirror state and
 * returns it BORROWED; sinks "logger.debug"/"print" record and return None.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "port_gw_session_store_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    gwss_store_t *store;
    json_t *cursors;
} script_ctx_t;

static void rec_ev(script_ctx_t *c, const char *name, const json_t *args, const json_t *ret) {
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", ret ? json_copy(ret) : json_null());
    json_append(c->trace, e);
}

static json_t *raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "message", json_string(msg));
    return r;
}

static json_t *stub(void *vctx, const char *name, const json_t *args) {
    script_ctx_t *c = vctx;
    /* structural: GatewayConnection.state(sid) get-or-create */
    if (strcmp(name, "connection.state") == 0) {
        const char *sid = json_get_str(args, "session_id", "");
        json_t *states = gwss_states(c->store);
        json_t *s = states ? json_obj_get(states, sid) : NULL;
        if (!s && states) { s = gwss_state_new(sid); json_set(states, sid, s); }
        json_t *proj = s ? gwss_state_proj(s) : json_null();
        rec_ev(c, name, args, proj);
        json_free(proj);
        return s; /* BORROWED mirror node — the flow mutates it in place */
    }
    /* sinks: record, return None, no script */
    if (strcmp(name, "logger.debug") == 0 || strcmp(name, "print") == 0) {
        rec_ev(c, name, args, NULL);
        return NULL;
    }
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *script = io ? json_obj_get(io, name) : NULL;
    json_t *result = NULL;
    if (!script) {
        char msg[300];
        snprintf(msg, sizeof(msg), "no script for %s", name);
        result = raise_spec("RuntimeError", msg);
    } else if (script->type == JSON_ARRAY) {
        json_t *cur = json_obj_get(c->cursors, name);
        size_t idx = (cur && cur->type == JSON_NUMBER) ? (size_t)cur->num_val : 0;
        if (idx >= script->c.count) {
            char msg[300];
            snprintf(msg, sizeof(msg), "script exhausted for %s", name);
            result = raise_spec("RuntimeError", msg);
        } else {
            result = json_copy(script->c.items[idx]);
            json_set(c->cursors, name, json_number((double)(idx + 1)));
        }
    } else if (script->type == JSON_OBJECT && gwss_truth(json_obj_get(script, "__script__"))) {
        result = gwss_copy(json_obj_get(script, "ret"));
    } else {
        result = json_copy(script);
    }
    rec_ev(c, name, args, result);
    return result;
}

/* ── state mirror seed ── */

static void seed_mirror(gwss_store_t *store, const json_t *in) {
    json_t *m = store->m;
    json_set(m, "snapshots", json_object());
    json_set(m, "current", json_null());
    json_set(m, "retry", json_null());
    json_t *conn = json_object();
    json_set(conn, "final", json_bool(false));
    json_set(conn, "launch_api_key", json_null());
    json_set(conn, "launch_api_key_typed", json_bool(false));
    json_set(conn, "states", json_object());
    json_set(m, "connection", conn);
    json_set(m, "reader", json_null());
    json_set(m, "handles", json_null());
    json_set(m, "persistence", json_null());
    json_set(m, "create_parameters", json_null());

    const json_t *stspec = json_obj_get(in, "st");
    if (!stspec || stspec->type != JSON_OBJECT) return;
    json_t *snaps = json_obj_get(stspec, "snapshots");
    if (snaps && snaps->type == JSON_OBJECT) json_set(m, "snapshots", json_copy(snaps));
    json_t *cspec = json_obj_get(stspec, "connection");
    if (cspec && cspec->type == JSON_OBJECT) {
        json_t *v;
        v = json_obj_get(cspec, "final");
        if (v) json_set(conn, "final", json_copy(v));
        v = json_obj_get(cspec, "launch_api_key");
        if (v) json_set(conn, "launch_api_key", json_copy(v));
        v = json_obj_get(cspec, "launch_api_key_typed");
        if (v) json_set(conn, "launch_api_key_typed", json_copy(v));
        json_t *sspec = json_obj_get(cspec, "states");
        if (sspec && sspec->type == JSON_OBJECT) {
            json_t *states = json_obj_get(conn, "states");
            for (size_t i = 0; i < sspec->c.count; i++) {
                const char *sid = sspec->c.keys[i];
                json_t *s = gwss_state_new(sid);
                json_t *fs = sspec->c.items[i];
                if (fs && fs->type == JSON_OBJECT) {
                    for (size_t j = 0;
                         j < sizeof(GWSS_STATE_FIELDS) / sizeof(GWSS_STATE_FIELDS[0]); j++) {
                        json_t *fv = json_obj_get(fs, GWSS_STATE_FIELDS[j]);
                        if (fv) json_set(s, GWSS_STATE_FIELDS[j], json_copy(fv));
                    }
                }
                json_set(states, sid, s);
            }
        }
    }
    /* current: {"ref": sid} means current IS snapshots[sid] (identity) */
    json_t *cur = json_obj_get(stspec, "current");
    if (cur && cur->type == JSON_OBJECT && cur->c.count == 1 && json_obj_get(cur, "ref")) {
        const char *ref = json_get_str(cur, "ref", "");
        json_t *sn = json_obj_get(gwss_snapshots(store), ref);
        json_set(m, "current", sn ? json_copy(sn) : json_null());
        store->current_ref = strdup(ref);
    } else if (cur) {
        json_set(m, "current", json_copy(cur));
    }
    json_t *retry = json_obj_get(stspec, "retry");
    if (retry && retry->type != JSON_NULL) json_set(m, "retry", json_copy(retry));
    json_t *v;
    v = json_obj_get(stspec, "reader");
    if (v) json_set(m, "reader", json_copy(v));
    v = json_obj_get(stspec, "handles");
    if (v) json_set(m, "handles", json_copy(v));
    v = json_obj_get(stspec, "persistence");
    if (v) json_set(m, "persistence", json_copy(v));
    v = json_obj_get(stspec, "create_parameters");
    if (v) json_set(m, "create_parameters", json_copy(v));
}

/* ── dispatch ── */

static void run_case(const char *func, const json_t *in, json_t *out) {
    /* reserve envelope key order: ret, trace, st */
    json_set(out, "ret", json_null());
    json_t *trace = json_array();
    json_set(out, "trace", trace);
    gwss_store_t store = { json_object(), NULL };
    seed_mirror(&store, in);
    script_ctx_t ctx = { in, trace, &store, json_object() };
    gwss_io_t io = { &ctx, stub };
    json_t *raise = NULL;
    json_t *ret = NULL;

    if (strcmp(func, "session_label") == 0) {
        ret = gwss_session_label(json_obj_get(in, "session_id"), &raise);
    } else if (strcmp(func, "_classic_only_policy") == 0) {
        ret = gwss_classic_only_policy(json_obj_get(in, "launch"), &raise);
    } else if (strcmp(func, "_transcript_view") == 0) {
        ret = gwss_transcript_view(json_obj_get(in, "include_inactive"),
                                   json_obj_get(in, "include_compacted"), &raise);
    } else if (strcmp(func, "_window") == 0) {
        ret = gwss_window(json_obj_get(in, "limit"), json_obj_get(in, "offset"), &raise);
    } else if (strcmp(func, "set_session_yolo") == 0) {
        ret = gwss_set_session_yolo(&store, in, &io, &raise);
    } else if (strcmp(func, "close_reader") == 0) {
        ret = gwss_close_reader(&store, in, &io, &raise);
    } else if (strcmp(func, "__getattr__") == 0) {
        ret = gwss_getattr(&store, in, &io, &raise);
    } else if (strcmp(func, "_call") == 0) {
        ret = gwss_call_flow(&store, in, &io, &raise);
    } else if (strcmp(func, "_adopt") == 0) {
        ret = gwss_adopt(&store, json_obj_get(in, "snapshot"), &io, &raise);
    } else if (strcmp(func, "attach") == 0) {
        ret = gwss_attach(&store, in, &io, &raise);
    } else if (strcmp(func, "open_session") == 0) {
        ret = gwss_open_session(&store, in, &io, &raise);
    } else if (strcmp(func, "create") == 0) {
        ret = gwss_create(&store, in, &io, &raise);
    } else if (strcmp(func, "branch") == 0) {
        ret = gwss_branch(&store, in, &io, &raise);
    } else if (strcmp(func, "_mutate") == 0) {
        ret = gwss_mutate(&store, json_obj_get(in, "session_id"),
                          json_obj_get(in, "operation"), json_obj_get(in, "payload"),
                          json_obj_get(in, "snapshot"), &io, &raise);
    } else if (strcmp(func, "set_session_title") == 0) {
        ret = gwss_set_session_title(&store, in, &io, &raise);
    } else if (strcmp(func, "delete_session") == 0) {
        ret = gwss_delete_session(&store, json_obj_get(in, "session_id"), &io, &raise);
    } else if (strcmp(func, "end_session") == 0) {
        ret = gwss_end_session(&store, in, &io, &raise);
    } else if (strcmp(func, "delete_session_if_empty") == 0) {
        ret = gwss_delete_session_if_empty(&store, in, &io, &raise);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    if (raise) {
        json_set(out, "ret", gwss_spec_emit(raise));
        json_free(raise);
        json_free(ret);
    } else {
        json_set(out, "ret", ret ? ret : json_null());
    }
    json_set(out, "st", json_copy(store.m));
    json_free(ctx.cursors);
    free(store.current_ref);
    json_free(store.m);
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