/*
 * t_port_console_engine_flow.c — oracle harness for the console-engine FLOW
 * cores + leaves ported in src/cli/console_engine_flow.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/console_engine_flow.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's "seams" object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). An uncaught raise parks in ce2_pending and is emitted as ret.
 * tests/oracle_console_engine_flow.py drives the LIVE Python with the same
 * scripts and compares the ordered decision sequence + return.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/console_engine_flow.c"

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
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (sc && sc->type == JSON_ARRAY) {
        size_t n = json_len(sc);
        if (n == 0) {
            char *m = xsf("no script for %s", name);
            result = make_raise("RuntimeError", m);
            free(m);
        } else {
            double idx = json_get_num(ctx->counters, name, 0);
            size_t i = (size_t)idx;
            result = json_copy(json_get(sc, i < n ? i : n - 1));
            json_set(ctx->counters, name, json_number(idx + 1));
        }
    } else if (sc) {
        result = json_copy(sc);
    } else {
        char *m = xsf("no script for %s", name);
        result = make_raise("RuntimeError", m);
        free(m);
    }
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)

static void ret_json(json_t *out, json_t *r) { json_set(out, "ret", r ? r : json_null()); }
static void ret_bool(json_t *out, bool b) { json_set(out, "ret", json_bool(b)); }
static void ret_str(json_t *out, char *s) {
    json_set(out, "ret", s ? json_string(s) : json_null());
    free(s);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    ce2_seams_t seams = {&ctx, script_call};
    const ce2_seams_t *sp = &seams;
    ce2_take_pending();  /* clear any stale parked raise */

    if (!strcmp(func, "paths")) {
        ret_json(out, ce2_paths(S("spec")));
    } else if (!strcmp(func, "sub")) {
        ret_json(out, ce2_sub(S("module"), S("builder"), S("handler")));
    } else if (!strcmp(func, "reg")) {
        ret_json(out, ce2_reg(S("module"), S("handler")));
    } else if (!strcmp(func, "send_surface")) {
        ret_json(out, ce2_send_surface());
    } else if (!strcmp(func, "shell_tokens")) {
        ret_bool(out, ce2_shell_tokens(S("token")));
    } else if (!strcmp(func, "confirmed_kinds")) {
        ret_bool(out, ce2_confirmed_kinds(S("kind")));
    } else if (!strcmp(func, "blocked_top")) {
        ret_bool(out, ce2_blocked_top(S("token")));
    } else if (!strcmp(func, "blocked_pairs")) {
        ret_str(out, ce2_blocked_pairs(J("tokens")));
    } else if (!strcmp(func, "drop_trailing_blank")) {
        json_t *lines = is_arr(J("lines")) ? json_copy(J("lines")) : json_array();
        ce2_drop_trailing_blank(lines, sp);
        ret_json(out, lines);
    } else if (!strcmp(func, "parse")) {
        ret_json(out, ce2_parse(S("prog"), J("args"), J("specs"), sp));
    } else if (!strcmp(func, "captured")) {
        ret_json(out, ce2_captured(J("fn")));
    } else if (!strcmp(func, "captured_wrapper")) {
        ret_json(out, ce2_captured_wrapper(J("fn"), J("engine"), J("args"), sp));
    } else if (!strcmp(func, "simple_command")) {
        ret_json(out, ce2_simple_command(S("usage"), S("module"), S("name")));
    } else if (!strcmp(func, "simple_command_handler")) {
        ret_json(out, ce2_simple_command_handler(S("usage"), S("module"), S("name"), J("args"), sp));
    } else if (!strcmp(func, "doctor")) {
        ret_json(out, ce2_doctor(J("args"), sp));
    } else if (!strcmp(func, "build_main_handler")) {
        ret_json(out, ce2_build_main_handler(S("handler"), B("live", false), sp));
    } else if (!strcmp(func, "surface_build")) {
        ret_json(out, ce2_surface_build(J("surface"), S("root"), B("live", false), sp));
    } else if (!strcmp(func, "surface_summaries")) {
        ret_json(out, ce2_surface_summaries(J("surface"), S("root"), sp));
    } else if (!strcmp(func, "dispatch_lambda")) {
        ret_json(out, ce2_dispatch_lambda(J("func"), J("namespace"), sp));
    } else if (!strcmp(func, "dispatch")) {
        bool nu = jtruth(J("namespace_update"));
        ret_json(out, ce2_dispatch(J("surface"), S("root"), J("fixed"), J("args"), nu, sp));
    } else if (!strcmp(func, "session_db")) {
        ret_json(out, ce2_session_db(B("read_only", true), sp));
    } else if (!strcmp(func, "guard_exports")) {
        ce2_guard_exports(J("store"), J("session_ids"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "cron_job_action")) {
        ret_json(out, ce2_cron_job_action(J("args"), S("usage"), S("action"), sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    json_t *pending = ce2_take_pending();
    if (pending) json_set(out, "ret", pending);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    json_free(ctx.counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) > 0) {
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case: %s\n", err ? err : "not object");
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
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    free(line);
    return 0;
}