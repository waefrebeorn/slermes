/*
 * t_port_file_tools_flow2.c — oracle harness for the tools/file_tools.py
 * UNCLAIMED remainder ported in src/tools/file_tools_flow2.c.
 *
 * Self-contained: includes the port .c by area-qualified path. Reads JSON-line
 * cases from stdin (tests/fixtures/file_tools_flow2.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (recording one
 * trace event per seam call, in order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED script semantics (identical to the oracle): a MISSING script is a
 * RuntimeError raise spec; a scripted null is NULL (None), never a json_null
 * node; an ARRAY script is a per-call queue (last repeats, empty -> None);
 * {"__raise__": cls, "msg": ...} scripts an exception. tests/oracle_file_tools_flow2.py
 * drives the LIVE Python with the same scripts and compares ordered trace + ret.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "tools/file_tools_flow2.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (!v || v->type == JSON_NULL) continue;
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
    if (!sc) {
        char *m = ft2_xsf("no script for seam: %s", name);
        result = ft2_raise("RuntimeError", m);
        free(m);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx, n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    /* NULL == None: a scripted null must NOT surface as a json_null node. */
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));   /* borrowed args: copy, do not free */
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define N(k, d) json_get_num(in, k, (double)(d))
#define B(k, d) json_get_bool(in, k, (bool)(d))

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    ft2_seams_t seams = {&ctx, script_call};
    const ft2_seams_t *sp = &seams;
    json_t *r = NULL;

    if (!strcmp(func, "apply_char_budget")) {
        /* result_dict is mutated in place -> pass a fresh copy. */
        json_t *rd = J("result") ? json_copy(J("result")) : json_object();
        r = ft2_apply_char_budget(sp, rd, S("content"), (long)N("offset", 1),
                                  J("total_lines"), (long)N("max_chars", 100000));
        json_free(rd);
    } else if (!strcmp(func, "resolved_match_path")) {
        r = ft2_resolved_match_path(sp, S("path"), S("task_id"));
    } else if (!strcmp(func, "resolve_or_none")) {
        r = ft2_resolve_or_none(sp, S("filepath"), S("task_id"), B("entry", false));
    } else if (!strcmp(func, "write_precheck_error")) {
        r = ft2_write_precheck_error(sp, J("paths"), J("content_paths"),
                                     S("task_id"), B("cross_profile", false));
    } else if (!strcmp(func, "edit_warnings")) {
        r = ft2_edit_warnings(sp, J("paths"), J("path_to_resolved"), S("task_id"));
    } else if (!strcmp(func, "note_edited")) {
        r = ft2_note_edited(sp, S("task_id"), J("paths"), J("path_to_resolved"), J("session_id"));
    } else if (!strcmp(func, "whole_file_rewrite_hint")) {
        r = ft2_whole_file_rewrite_hint(sp, S("task_id"), S("resolved"), S("new_content"));
    } else if (!strcmp(func, "collect_v4a_header_paths")) {
        r = ft2_collect_v4a_header_paths(sp, S("patch"));
    } else if (!strcmp(func, "owned")) {
        r = ft2_owned(sp, S("owner"), J("call_kwargs"));
    } else if (!strcmp(func, "read_device_base")) {
        r = ft2_read_device_base(sp, S("path"), S("task_id"));
    } else if (!strcmp(func, "dedup_stub_or_block")) {
        /* task_data mutated -> pass a fresh copy. */
        json_t *td = J("task_data") ? json_copy(J("task_data")) : json_object();
        r = ft2_dedup_stub_or_block(sp, td, J("dedup_key"), S("path"));
        json_free(td);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    ret_json(out, r);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    json_free(ctx.counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
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
            free(err); json_free(c); continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        /* Reserve envelope key positions: canon() treats insertion order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
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
        json_free(out); json_free(c);
    }
    free(line);
    return 0;
}
