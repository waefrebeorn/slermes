/*
 * t_port_session_transcript_rows_flow2.c — oracle harness for the FLOW port
 * src/gateway/session_transcript_rows_flow2.c (7 UNCLAIMED defs of
 * gateway/session_transcript_rows.py).
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func":..., "in":{...}}.
 * "in" carries call args + "io" (scripted seam results). The universal seam
 * stub records one {ev, args, ret} event per call in call order. The harness
 * emits {"func","in","out":{"ret":...,"trace":[...]}} per case.
 * tests/oracle_session_transcript_rows_flow2.py drives the LIVE Python module
 * with the same scripts and diffs ret + trace.
 *
 * Script semantics (shared with the oracle): a bare JSON array is ONE RESULT
 * PER CALL (last value repeats); an empty array is constant None;
 * {"__script__":true,"ret":v} is a constant; anything else is a constant;
 * absent == None. {"__raise__":kind,"message":m} scripts a raised exception.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gateway/session_transcript_rows_flow2.c"

typedef struct {
    const json_t *io;    /* borrowed in["io"] */
    json_t *trace;       /* owned array */
    json_t *pos;         /* owned per-name queue positions */
} str2_script_ctx;

/* Script lookup: bare array = per-call queue (last repeats); empty array =
 * constant None; {"__script__":true,"ret":v} = constant; else constant. */
static const json_t *str2_next_script(str2_script_ctx *c, const char *name) {
    const json_t *v = c->io ? json_obj_get(c->io, name) : NULL;
    if (!v) return NULL;
    if (v->type == JSON_ARRAY) {
        if (v->c.count == 0) return NULL;
        double idx = json_get_num(c->pos, name, 0);
        size_t i = (size_t)idx;
        if (i >= v->c.count) i = v->c.count - 1;
        else json_set(c->pos, name, json_number((double)(i + 1)));
        return json_get(v, i);
    }
    if (v->type == JSON_OBJECT && json_get_bool(v, "__script__", false))
        return json_obj_get(v, "ret");
    return v;
}

static json_t *str2_stub_call(void *ud, const char *name, const json_t *args) {
    str2_script_ctx *c = ud;
    const json_t *script = str2_next_script(c, name);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_null());
    json_set(ev, "ret", script ? json_copy(script) : json_null());
    json_append(c->trace, ev);
    if (!script || script->type == JSON_NULL) return NULL;   /* None == NULL */
    return json_copy(script);
}

typedef json_t *(*str2_entry_fn)(str2_env *, const json_t *);
typedef struct { const char *name; str2_entry_fn fn; } str2_dispatch;

static const str2_dispatch str2_table[] = {
    { "handoff_row", str2_handoff_row },
    { "_mark_index", str2_mark_index },
    { "handoff_occurrence", str2_handoff_occurrence },
    { "shown_occurrences", str2_shown_occurrences },
    { "handoff_rows", str2_handoff_rows },
    { "page_handoffs", str2_page_handoffs },
    { "project_entries", str2_project_entries },
};

static void str2_run_case(const char *func, const json_t *in, json_t *out) {
    str2_script_ctx ctx = {
        .io = json_obj_get(in, "io"),
        .trace = json_array(), .pos = json_object(),
    };
    json_t *state = json_object();
    str2_env env = { .state = state, .call = str2_stub_call, .ud = &ctx,
                     .exc = NULL, .cur = NULL };
    json_t *ret = NULL;
    bool found = false;
    for (size_t i = 0; i < sizeof(str2_table) / sizeof(str2_table[0]); i++) {
        if (strcmp(func, str2_table[i].name) == 0) {
            ret = str2_table[i].fn(&env, in);
            found = true;
            break;
        }
    }
    if (!found) json_set(out, "error", json_string("unknown func"));
    if (env.exc) {   /* propagated raise */
        json_free(ret);
        ret = env.exc;
    }
    if (env.cur) json_free(env.cur);
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", ctx.trace);   /* adopts */
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
        str2_run_case(func, in ? in : empty, out);
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
