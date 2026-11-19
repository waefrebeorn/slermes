/*
 * t_port_terminal_hints_flow.c — oracle harness for the tools/terminal_hints.py
 * UNCLAIMED-remainder FLOW ported in src/tools/terminal_hints_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One generic recording seam serves every flow: it looks up the case's
 * in.io[<seam name>] script, records {"ev": name, "args": ..., "ret": script}
 * in call order, and returns the scripted value. Script shapes:
 *   - a plain JSON value: returned on EVERY call (ret marker = the value)
 *   - {"__seq__": [v1, v2, ...]}: consumed one per call; after exhaustion the
 *     LAST element repeats; empty -> None
 *   - {"__raise__": kind, "msg": text}: returned as-is; the port detects it
 *   - a scripted JSON null: returns a NULL pointer (Python None)
 *   - ABSENT key: FAIL-CLOSED — returns {"__raise__":"RuntimeError",
 *     "msg":"no script for <name>"} (recorded as ret), so an unscripted seam
 *     raises rather than silently reading None.
 *
 * Emits {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...]}};
 * tests/oracle_terminal_hints_flow.py drives the LIVE Python with the same
 * scripts and diffs the ordered trace + ret. No module state is mutated
 * (the ported defs are pure leaves / closure flows).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "tools/terminal_hints_flow.c"

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
        /* fail-closed: no script for this seam */
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

/* Read a string-or-null arg: JSON_STRING -> its value; null/absent -> NULL
 * (models Python None). Non-string scalars are outside the fixture domain. */
static const char *sval_or_null(const json_t *in, const char *k) {
    const json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    th_seams_t s = { &ctx, script_call };
    json_t *r = NULL;
    const char *command = json_get_str(in, "command", "");
    const char *output = sval_or_null(in, "output");

    if (strcmp(func, "regex_hint") == 0) {
        r = th_regex_hint(&s, json_get_str(in, "pattern", ""),
                          node(in, "message"), (int)json_get_num(in, "flags", 0));
    } else if (strcmp(func, "hint") == 0) {
        r = th_hint(&s, node(in, "message"), command, output);
    } else if (strcmp(func, "hint_gh_unknown_json_field") == 0) {
        r = th_hint_gh_unknown_json_field(&s, command, output);
    } else if (strcmp(func, "hint_merge_conflict") == 0) {
        r = th_hint_merge_conflict(&s, command, output);
    } else if (strcmp(func, "hint_command_not_found") == 0) {
        r = th_hint_command_not_found(&s, command, output);
    } else if (strcmp(func, "hint_module_not_found") == 0) {
        r = th_hint_module_not_found(&s, command, output);
    } else if (strcmp(func, "hint_already_exists") == 0) {
        r = th_hint_already_exists(&s, command, output);
    } else if (strcmp(func, "hint_gh_rate_limit") == 0) {
        r = th_hint_gh_rate_limit(&s, command, output);
    } else if (strcmp(func, "hint_permission_denied") == 0) {
        r = th_hint_permission_denied(&s, command, output);
    } else if (strcmp(func, "missing_command_hint") == 0) {
        r = th_missing_command_hint(json_get_str(in, "missing", ""));
    } else if (strcmp(func, "wrapper_cd_hint") == 0) {
        r = th_wrapper_cd_hint(&s, output);
    } else if (strcmp(func, "failure_shapes_search") == 0) {
        r = th_failure_shapes_search(&s, json_get_str(in, "window", ""));
    } else if (strcmp(func, "masking_pipe_search") == 0) {
        r = th_masking_pipe_search(&s, json_get_str(in, "cmd", ""));
    } else if (strcmp(func, "masking_fallback_search") == 0) {
        r = th_masking_fallback_search(&s, json_get_str(in, "cmd", ""));
    } else if (strcmp(func, "wrapper_cd_fullmatch") == 0) {
        r = th_wrapper_cd_fullmatch(&s, json_get_str(in, "last_line", ""));
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
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