/*
 * t_port_memory_tool_flow4.c — oracle harness for the tools/memory_tool.py
 * final UNCLAIMED-remainder FLOW core ported in src/tools/memory_tool_flow4.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Calls mt4_run() and emits
 *   {"func": ..., "in": ..., "out": {"value": ..., "payload": ..., "trace": [...]}}.
 * The FLOW trace is the ordered scripted-seam event list; all sides (C, live
 * Python, shadow mirror) use identical event shapes.
 * tests/oracle_memory_tool_flow4.py drives the LIVE extracted defs with the
 * same inputs and diffs the ordered trace + projected value.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools/memory_tool_flow4.c"

static void run_case(const char *func, const json_t *in, json_t *out) {
    json_t *r = mt4_run(func, in);
    json_set(out, "value", json_copy(json_obj_get(r, "value")));
    json_set(out, "payload", json_copy(json_obj_get(r, "payload")));
    json_set(out, "trace", json_copy(json_obj_get(r, "trace")));
    json_free(r);
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
        json_t *fn = json_string(func);
        char *func_s = json_serialize(fn);
        json_free(fn);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}
