/*
 * t_port_scale_to_zero_flow.c — oracle harness for the gateway/scale_to_zero.py
 * UNCLAIMED-remainder FLOW cores ported in src/gateway/scale_to_zero_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One generic recording seam serves every flow: it looks up the case's
 * in.io[<seam name>] script, records {"ev": name, "args": ..., "ret": script}
 * in call order, and returns the scripted value. Script shapes:
 *   - a plain JSON value: returned on EVERY call (ret marker = the value)
 *   - {"__seq__": [v1, v2, ...]}: consumed one per call; after exhaustion the
 *     LAST element repeats; empty -> None
 *   - {"__raise__": kind, "msg": text, ...}: returned as-is; the port detects it
 *   - a scripted JSON null: returns a NULL pointer (Python None)
 *   - ABSENT key: FAIL-CLOSED — returns {"__raise__":"RuntimeError",
 *     "msg":"no script for <name>"} (recorded as ret), so an unscripted seam
 *     raises rather than silently reading None.
 *
 * Emits {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...],
 * "state": {"malformed_logged": bool}}}; tests/oracle_scale_to_zero_flow.py
 * drives the LIVE Python with the same scripts and diffs ordered trace + ret +
 * latch state. The latch state is materialized per case from in.state and echoed
 * for EVERY func (mutation history preserved even on a raise).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/scale_to_zero_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *seq_pos;   /* seam name -> next sequence index */
} script_ctx_t;

static json_t *script_call(void *ctx, const char *name, const json_t *args) {
    script_ctx_t *c = (script_ctx_t *)ctx;
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *script = io ? json_obj_get(io, name) : NULL;
    const json_t *val = script;
    if (script && script->type == JSON_OBJECT) {
        const json_t *seq = json_obj_get(script, "__seq__");
        if (seq && seq->type == JSON_ARRAY) {
            const json_t *pos = json_obj_get(c->seq_pos, name);
            size_t idx = (pos && pos->type == JSON_NUMBER) ? (size_t)pos->num_val : 0;
            size_t n = json_len(seq);
            if (n == 0) {
                val = NULL;
            } else {
                if (idx >= n) idx = n - 1;
                val = json_get(seq, idx);
                json_set(c->seq_pos, name, json_number((double)(idx + 1)));
            }
        }
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    if (!script) {
        json_t *rs = json_object();
        json_set(rs, "__raise__", json_string("RuntimeError"));
        char m[160];
        snprintf(m, sizeof(m), "no script for %s", name);
        json_set(rs, "msg", json_string(m));
        json_set(e, "ret", json_copy(rs));
        json_append(c->trace, e);
        return rs;
    }
    json_set(e, "ret", val ? json_copy(val) : json_null());
    json_append(c->trace, e);
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

static const json_t *node(const json_t *in, const char *k) {
    return json_obj_get(in, k);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    stz_seams_t s = { &ctx, script_call };
    json_t *r = NULL;

    /* per-case latch state, materialized from in.state (default false) */
    const json_t *inst = node(in, "state");
    json_t *state = inst ? json_copy(inst) : json_object();
    if (state->type != JSON_OBJECT) { json_free(state); state = json_object(); }
    if (!json_has(state, "malformed_logged"))
        json_set(state, "malformed_logged", json_bool(false));

    const json_t *env = node(in, "environ");
    if (strcmp(func, "env_str") == 0) {
        r = stz_env_str(node(in, "env"), json_get_str(in, "key", ""), &s);
    } else if (strcmp(func, "self_suspend_available") == 0) {
        r = stz_self_suspend_available(env, &s);
    } else if (strcmp(func, "brokered_sleep_url") == 0) {
        r = stz_brokered_sleep_url(env, state, &s);
    } else if (strcmp(func, "suspend_available") == 0) {
        r = stz_suspend_available(env, state, &s);
    } else if (strcmp(func, "request_brokered_suspend") == 0) {
        r = stz_request_brokered_suspend(json_get_str(in, "url", ""),
                                         json_get_num(in, "timeout", 40.0),
                                         node(in, "opener"), &s);
    } else if (strcmp(func, "suspend_self") == 0) {
        r = stz_suspend_self(env, json_get_str(in, "socket_path", "/.fly/api"),
                             json_get_num(in, "timeout", 10.0), &s);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", state);
    json_free(ctx.seq_pos);
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
