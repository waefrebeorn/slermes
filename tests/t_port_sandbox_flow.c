/*
 * t_port_sandbox_flow.c — oracle harness for the sandbox-scope FLOW surface
 * ported in src/tools/sandbox_flow.c (tools/environments/sandbox_scope.py).
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires the universal seam to a SCRIPTED recording stub (in["io"][name]: a
 * single value reused, or a non-empty array consumed per call with the last
 * value repeating) that appends one {ev,args,ret} trace event per call, in
 * call order. FAIL-CLOSED: a MISSING script key or an EMPTY array returns a
 * {"__raise__":"RuntimeError","msg":"no script for <name>"} spec, recorded as
 * the event ret before the port propagates it. A scripted JSON null returns
 * a NULL pointer (None). Emits {"func","in","out":{ret,exc,trace[,state]}}
 * per case; the original "in" is echoed unmutated. env_setattr runs on a COPY
 * of in["env"] and echoes the mutated record as out["state"].
 * tests/oracle_sandbox_flow.py drives the LIVE Python with the same scripts.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "libjson/json.h"
#include "tools/sandbox_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;    /* owned JSON_ARRAY */
    json_t *counts;   /* object: seam name -> call count (array scripts) */
} sb_script_ctx_t;

/* ── generic scripted recording seam (fail-closed) ── */
static json_t *sb_sc_call(void *ctx, const char *name, const json_t *args) {
    sb_script_ctx_t *c = ctx;
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *script = io ? json_obj_get(io, name) : NULL;
    const json_t *val = NULL;
    bool fail = false;
    if (!script) {
        fail = true;                      /* MISSING key (present null is a script) */
    } else if (script->type == JSON_ARRAY) {
        if (script->c.count == 0) {
            fail = true;                  /* exhausted / empty sequence */
        } else {
            double cnt = json_get_num(c->counts, name, 0);
            size_t idx = (size_t)cnt;
            if (idx >= script->c.count) idx = script->c.count - 1;
            val = script->c.items[idx];
            json_set(c->counts, name, json_number(cnt + 1));
        }
    } else {
        val = script;                     /* single value reused */
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    if (fail) {
        char msg[300];
        snprintf(msg, sizeof(msg), "no script for %s", name);
        json_t *spec = sb_raise("RuntimeError", msg);
        json_set(e, "ret", json_copy(spec));
        json_append(c->trace, e);
        return spec;
    }
    json_set(e, "ret", val ? json_copy(val) : json_null());
    json_append(c->trace, e);
    /* a scripted JSON null returns a NULL pointer (None), not a json_null node */
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

/* ── input readers ── */
static const json_t *nd(const json_t *in, const char *k) { return json_obj_get(in, k); }
static const char *st(const json_t *in, const char *k) { return json_get_str(in, k, ""); }
static double nm(const json_t *in, const char *k, double def) { return json_get_num(in, k, def); }

/* ── per-case dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    sb_script_ctx_t ctx = { in, json_array(), json_object() };
    sb_seams_t s = { &ctx, sb_sc_call };
    json_t *r = NULL;
    json_t *state = NULL;
    const json_t *env = nd(in, "env");
    const json_t *pst = nd(in, "pipe_state");

    if (strcmp(func, "hosted_scope") == 0) r = sb_hosted_scope(&s);
    else if (strcmp(func, "sandbox_identity") == 0) r = sb_sandbox_identity(nd(in, "scope"));
    else if (strcmp(func, "identity_key") == 0) r = sb_identity_key(st(in, "tenant"), st(in, "agent"));
    else if (strcmp(func, "current_sandbox_key") == 0) r = sb_current_sandbox_key(&s);
    else if (strcmp(func, "sandbox_visible") == 0) r = sb_sandbox_visible(&s, nd(in, "sandbox_key"));
    else if (strcmp(func, "sandbox_lexical_path") == 0) r = sb_sandbox_lexical_path(&s, nd(in, "path"));
    else if (strcmp(func, "producer_fields") == 0) r = sb_producer_fields(nd(in, "sandbox_key"));
    else if (strcmp(func, "event_visible") == 0) r = sb_event_visible(&s, nd(in, "event"));
    else if (strcmp(func, "require_identity") == 0) r = sb_require_identity(&s, nd(in, "identity"), st(in, "what"));
    else if (strcmp(func, "admit_env") == 0) r = sb_admit_env(&s, nd(in, "env"));
    else if (strcmp(func, "create_hosted_environment") == 0)
        r = sb_create_hosted_environment(&s, st(in, "env_type"), nd(in, "provider"), nd(in, "kwargs"));
    else if (strcmp(func, "accepts_scope") == 0) r = sb_accepts_scope(nd(in, "method"));
    else if (strcmp(func, "require_hosted_interface") == 0) r = sb_require_hosted_interface(nd(in, "inner"), st(in, "env_type"));
    else if (strcmp(func, "scoped_call") == 0)
        r = sb_scoped_call(&s, nd(in, "identity"), st(in, "what"), nd(in, "args"), nd(in, "kwargs"));
    else if (strcmp(func, "env_init") == 0)
        r = sb_env_init(nd(in, "inner"), st(in, "inner_repr"), nd(in, "identity"), nd(in, "owner"));
    else if (strcmp(func, "env_getattr") == 0) r = sb_env_getattr(env, st(in, "name"));
    else if (strcmp(func, "env_setattr") == 0) {
        json_t *ec = env ? json_copy(env) : json_object();
        r = sb_env_setattr(ec, st(in, "name"), nd(in, "value"));
        state = ec;
    }
    else if (strcmp(func, "env_guarded") == 0)
        r = sb_env_guarded(&s, env, st(in, "name"), nd(in, "method"), nd(in, "args"), nd(in, "kwargs"));
    else if (strcmp(func, "env_lifecycle") == 0)
        r = sb_env_lifecycle(&s, env, st(in, "name"), nd(in, "method"), nd(in, "args"), nd(in, "kwargs"));
    else if (strcmp(func, "env_start_process") == 0)
        r = sb_env_start_process(&s, env, st(in, "name"), nd(in, "argv"), nd(in, "child_env"));
    else if (strcmp(func, "env_attach_process") == 0) r = sb_env_attach_process(&s, env, nd(in, "ref"));
    else if (strcmp(func, "env_stop_process") == 0) r = sb_env_stop_process(&s, env, nd(in, "ref"));
    else if (strcmp(func, "env_guard_stream") == 0) r = sb_env_guard_stream(env, nd(in, "stream"), nd(in, "name"));
    else if (strcmp(func, "env_own_name") == 0) r = sb_env_own_name(env, nd(in, "ref"), st(in, "what"));
    else if (strcmp(func, "env_repr") == 0) r = sb_env_repr(env);
    else if (strcmp(func, "pipe_op") == 0) r = sb_pipe_op(&s, pst, st(in, "what"));
    else if (strcmp(func, "pipe_read") == 0) r = sb_pipe_read(&s, pst, nm(in, "size", -1));
    else if (strcmp(func, "pipe_read1") == 0) r = sb_pipe_read1(&s, pst, nm(in, "size", -1));
    else if (strcmp(func, "pipe_readline") == 0) r = sb_pipe_readline(&s, pst, nm(in, "size", -1));
    else if (strcmp(func, "pipe_readlines") == 0) r = sb_pipe_readlines(&s, pst, nm(in, "hint", -1));
    else if (strcmp(func, "pipe_readinto") == 0) r = sb_pipe_readinto(&s, pst, nd(in, "buffer"));
    else if (strcmp(func, "pipe_write") == 0) r = sb_pipe_write(&s, pst, nd(in, "data"));
    else if (strcmp(func, "pipe_writelines") == 0) r = sb_pipe_writelines(&s, pst, nd(in, "lines"));
    else if (strcmp(func, "pipe_flush") == 0) r = sb_pipe_flush(&s, pst);
    else if (strcmp(func, "pipe_close") == 0) r = sb_pipe_close(&s, pst);
    else if (strcmp(func, "pipe_closed") == 0) r = sb_pipe_closed(&s, pst);
    else if (strcmp(func, "pipe_readable") == 0) r = sb_pipe_readable(&s, pst);
    else if (strcmp(func, "pipe_writable") == 0) r = sb_pipe_writable(&s, pst);
    else if (strcmp(func, "pipe_setattr") == 0) r = sb_pipe_setattr(st(in, "name"));
    else if (strcmp(func, "pipe_iter") == 0) r = sb_pipe_iter(&s, pst);
    else if (strcmp(func, "pipe_next") == 0) r = sb_pipe_next(&s, pst);
    else if (strcmp(func, "pipe_enter") == 0) r = sb_pipe_enter(&s, pst);
    else if (strcmp(func, "pipe_exit") == 0) r = sb_pipe_exit(&s, pst);
    else if (strcmp(func, "pipe_repr") == 0) r = sb_pipe_repr(pst);
    else r = sb_raise("ValueError", "unknown func");

    if (sb_is_raise(r)) {
        json_set(out, "ret", json_null());
        json_set(out, "exc", r);
    } else {
        json_set(out, "ret", r ? r : json_null());
        json_set(out, "exc", json_null());
    }
    json_set(out, "trace", ctx.trace);
    if (state) json_set(out, "state", state);
    json_free(ctx.counts);
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
            free(err); json_free(c); continue;
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
