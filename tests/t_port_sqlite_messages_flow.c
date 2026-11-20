/*
 * t_port_sqlite_messages_flow.c — oracle harness for the
 * persistence/sqlite/conversation/messages.py FLOW + leaf ports in
 * src/persistence/sqlite_messages_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires the universal seam to the SCRIPTED results in in.io (recording stubs
 * appending one trace event per seam call, in call order) and emits
 * {"func","in","out":{"ret","trace"}} per case.
 * tests/oracle_sqlite_messages_flow.py drives the LIVE Python with the same
 * scripts and compares the ordered decision sequence + return (or raise).
 *
 * Script semantics (identical on the oracle side):
 *   io[name] absent        -> RuntimeError "no script for <name>" (fail-closed)
 *   io[name] = array       -> per-call queue (exhaustion -> RuntimeError)
 *   io[name] = {"__script__":true,"ret":X} -> constant container return X
 *   io[name] = value       -> constant return (deep-copied per call)
 *   {"__raise__":cls,"message":msg} -> a raised exception (KeyError msg is the
 *   RAW key; an int key carries "key_kind":"int"; str(KeyError(k))==repr(k)
 *   is applied once at ret emit). Specs may carry "cause" (nested spec).
 * STRUCTURAL seams "refusals.enter"/"refusals.exit" and
 * "_execute_transcript_write.enter"/".exit" record and return None without a
 * script (the seamed CMs are passthroughs).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "persistence/sqlite_messages_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;
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

static bool structural(const char *name) {
    return strcmp(name, "refusals.enter") == 0
        || strcmp(name, "refusals.exit") == 0
        || strcmp(name, "_execute_transcript_write.enter") == 0
        || strcmp(name, "_execute_transcript_write.exit") == 0;
}

static json_t *stub(void *vctx, const char *name, const json_t *args) {
    script_ctx_t *c = vctx;
    /* structural: the seamed refusals() / _execute_transcript_write CMs */
    if (structural(name)) {
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
    } else if (script->type == JSON_OBJECT && sm_truth(json_obj_get(script, "__script__"))) {
        result = sm_cp(json_obj_get(script, "ret"));
    } else {
        result = json_copy(script);
    }
    rec_ev(c, name, args, result);
    return result;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    /* reserve envelope key order: ret, trace */
    json_set(out, "ret", json_null());
    json_t *trace = json_array();
    json_set(out, "trace", trace);
    script_ctx_t ctx = { in, trace, json_object() };
    sm_io_t io = { &ctx, stub };
    json_t *raise = NULL;
    json_t *ret = sm_dispatch(func, in, &io, &raise);
    if (raise) {
        json_set(out, "ret", sm_exc_project(raise));
        json_free(raise);
        json_free(ret);
    } else {
        json_set(out, "ret", ret ? ret : json_null());
    }
    json_free(ctx.cursors);
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
        /* Echo the PRISTINE input: the port applies stamp scripts to its
         * working dicts IN PLACE, and the oracle drives live with this
         * echoed "in" -- serializing after the run would feed it pre-stamped
         * inputs (enter-event args + the _row_id repair gate diverge). */
        char *in_s = json_serialize(in);
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
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