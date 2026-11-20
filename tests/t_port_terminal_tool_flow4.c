/*
 * t_port_terminal_tool_flow4.c — oracle harness for the tools/terminal_tool.py
 * final UNCLAIMED-remainder FLOW core ported in src/tools/terminal_tool_flow4.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Dispatches to the static tt4_* entry points and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...]}}.
 * The FLOW trace carries the ordered per-clause isinstance checks for the
 * catch_dispatch func; all other funcs emit an empty trace.
 * tests/oracle_terminal_tool_flow4.py drives the LIVE Python with the same
 * inputs and diffs the ordered trace + return (and a shadow mirror of this C
 * algorithm) against the harness output.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools/terminal_tool_flow4.c"

static const json_t *node(const json_t *in, const char *k) {
    return json_obj_get(in, k);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    json_t *trace = json_array();
    json_t *r = NULL;

    if (strcmp(func, "rejected_init") == 0) {
        r = tt4_rejected_init(json_get_str(in, "result_json", ""));
    } else if (strcmp(func, "isinstance") == 0) {
        r = tt4_isinstance(json_get_str(in, "kind", ""));
    } else if (strcmp(func, "hierarchy") == 0) {
        r = tt4_hierarchy(node(in, "names"));
    } else if (strcmp(func, "catch_dispatch") == 0) {
        r = tt4_catch(json_get_str(in, "result_json", ""), node(in, "clauses"), trace);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", trace);
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
