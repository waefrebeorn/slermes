/*
 * t_port_skill_usage_flow.c — oracle harness for the unclaimed tools/skill_usage.py
 * FLOW cores + leaves ported in src/cli/skill_usage_flow.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/skill_usage_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":{...}}}.
 * Injected dicts (rec / data / held) are mutated in place and echoed in out.state.
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (Python None).
 * tests/oracle_skill_usage_flow.py drives the LIVE Python with the same scripts
 * and compares the ordered decision sequence + return + mutated state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/skill_usage_flow.c"

typedef struct {
    const json_t *in;
    su_io_t *io;
    json_t *trace;     /* array of events */
    json_t *counters;
    json_t *data_snap; /* _locked_update data snapshot (captured at op call) */
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
            char *m = su_fmt("no script for %s", name);
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
        char *m = su_fmt("no script for %s", name);
        result = make_raise("RuntimeError", m);
        free(m);
    }
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    /* logger.debug is an inert recording event (RecLogger never raises); it
     * never fail-closes, even when unscripted. */
    if (!strcmp(name, "logger.debug")) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define SD(k, d) json_get_str(in, k, d)
#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)
#define N(k, d) json_get_num(in, k, d)

static void ret_json(json_t *out, json_t *r) { json_set(out, "ret", r ? r : json_null()); }

static json_t *copy_obj_field(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object();
}
static json_t *copy_arr_field(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_ARRAY) ? json_copy(v) : json_array();
}

/* _locked_update callbacks (seamed for direct drive). */
static json_t *guard_cb(void *vctx) {
    script_ctx_t *ctx = vctx;
    return su_call(ctx->io, "guard", json_object());
}
static json_t *op_cb(void *vctx, json_t *data) {
    script_ctx_t *ctx = vctx;
    json_free(ctx->data_snap);
    ctx->data_snap = json_copy(data);
    json_t *a = json_object();
    json_set(a, "data", json_copy(data));
    return su_call(ctx->io, "op", a);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, NULL, json_object(), NULL};
    su_io_t io = {script_call, &ctx, J("env")};
    ctx.io = &io;
    json_t *state = NULL;
    json_t *r = NULL;

    if (!strcmp(func, "int_or_zero")) {
        r = su_int_or_zero(J("value"));
    } else if (!strcmp(func, "read_lines")) {
        r = su_read_lines_pub(&io, SD("path", ""), SD("fail_log", ""));
    } else if (!strcmp(func, "backfilled")) {
        r = su_backfilled_pub(&io, J("rec"));
    } else if (!strcmp(func, "report_row")) {
        r = su_report_row(&io, SD("name", ""), J("raw"), J("extra"));
    } else if (!strcmp(func, "load_usage_strict")) {
        r = su_load_usage_strict_pub(&io);
    } else if (!strcmp(func, "is_locked")) {
        r = su_is_locked(&io, SD("skill_name", ""));
    } else if (!strcmp(func, "locked_skill_names")) {
        r = su_locked_skill_names(&io, B("strict", false));
    } else if (!strcmp(func, "match_skill_dir")) {
        r = su_match_skill_dir(&io, J("skill_mds"), SD("skill_name", ""));
    } else if (!strcmp(func, "is_external_only")) {
        r = su_is_external_only(&io, SD("skill_name", ""));
    } else if (!strcmp(func, "external_skill_names")) {
        r = su_external_skill_names(&io);
    } else if (!strcmp(func, "iter_skill_mds")) {
        r = su_iter_skill_mds_pub(&io, SD("base", ""), B("local_only", true));
    } else if (!strcmp(func, "read_bundled_names")) {
        r = su_read_bundled_names_pub(&io);
    } else if (!strcmp(func, "toggle_suppressed_name")) {
        r = su_toggle_suppressed_name_pub(&io, SD("skill_name", ""), B("add", false));
    } else if (!strcmp(func, "scan_local_skills")) {
        r = su_scan_local_skills(&io);
    } else if (!strcmp(func, "bump")) {
        json_t *rec = copy_obj_field(in, "rec");
        r = su_bump(&io, rec, SD("count_key", ""), SD("ts_key", ""));
        state = json_object();
        json_set(state, "rec", rec);
    } else if (!strcmp(func, "locked_update")) {
        r = su_locked_update(&io, SD("skill_name", ""), op_cb, &ctx, SD("fail_log", ""),
                             B("has_guard", false) ? guard_cb : NULL, &ctx);
        state = json_object();
        json_set(state, "data", ctx.data_snap ? ctx.data_snap : json_object());
        ctx.data_snap = NULL;
    } else if (!strcmp(func, "set_field")) {
        r = su_set_field(&io, SD("skill_name", ""), SD("key", ""), J("value"));
    } else if (!strcmp(func, "mutate_and_emit")) {
        r = su_mutate_and_emit(&io, SD("skill_name", ""), SD("action", ""),
                               SD("mutator", ""), J("hook_kwargs"));
    } else if (!strcmp(func, "reanchor_clock")) {
        r = su_reanchor_clock(&io, SD("skill_name", ""));
    } else if (!strcmp(func, "set_locked")) {
        r = su_set_locked(&io, SD("skill_name", ""), B("locked", false));
    } else if (!strcmp(func, "flock")) {
        r = su_flock(&io, SD("fd", ""), B("lock", true), B("fcntl", false), B("msvcrt", false));
    } else if (!strcmp(func, "skill_file_lock")) {
        json_t *held = copy_arr_field(in, "held");
        int has_paths = B("has_paths", false);
        int fcntl = B("fcntl", false), msvcrt = B("msvcrt", false);
        json_t *lctx = NULL;
        r = su_skill_file_lock_enter(&io, SD("lock_path", ""), held, &has_paths,
                                     fcntl, msvcrt, &lctx);
        if (!su_is_raise(r)) {
            json_t *body = su_call(&io, "body", json_object());
            json_t *body_exc = NULL;
            if (su_is_raise(body)) body_exc = body;
            else json_free(body);
            json_t *ex = su_skill_file_lock_exit(&io, lctx, held, fcntl, msvcrt);
            if (su_is_raise(ex)) { json_free(body_exc); r = ex; }
            else { json_free(ex); r = body_exc; }
        }
        json_free(lctx);
        state = json_object();
        json_set(state, "held", su_sorted_unique(held));
        json_set(state, "has_paths", json_bool(has_paths ? true : false));
        json_free(held);
    } else if (!strcmp(func, "relocate")) {
        r = su_relocate(&io, SD("src", ""), SD("dest", ""), SD("skill_name", ""),
                        SD("action", ""), J("capture_kwargs"));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        r = make_raise("RuntimeError", "unknown func");
    }

    ret_json(out, r);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    if (state) json_set(out, "state", state);
    json_free(ctx.data_snap);
    json_free(ctx.counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
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