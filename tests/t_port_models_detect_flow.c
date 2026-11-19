/* t_port_models_detect_flow.c — harness for the hermes_cli/models_detect.py
 * FLOW port (src/cli/models_detect_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}} where "in" carries
 * the call arguments plus "env" (scripted tables/constants) and "io" (seam-name
 * -> script: single value, {"$seq": [...]} per-call queue, or
 * {"__raise__": cls, "msg": m}). Emits JSON LINES:
 *   {"func", "in", "out", "trace"}
 * "out" is the canonical return (None -> null, bool -> bool, uncaught exception
 * -> {"__raise__": cls}); "trace" is the ordered seam-call sequence
 * [{"ev","args","ret"}]. The oracle drives LIVE models_detect.py under the SAME
 * scripts and diffs out + trace.
 *
 * Self-contained: links libjson alone (the port is #included below).
 */
#include "hermes_json.h"
#include "cli/models_detect_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 16];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) {
            fprintf(stderr, "harness: unparseable case line\n");
            return 1;
        }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *empty_in = NULL;
        if (!in) { empty_in = json_object(); in = empty_in; }
        mdet_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *ret = mdet_run(&ctx, func, in);
        json_t *out;
        if (ctx.exc) {
            out = json_object();
            json_set(out, "__raise__", json_copy(json_obj_get(ctx.exc, "__raise__")));
            json_free(ctx.exc);
            json_free(ret);
        } else {
            out = ret ? ret : json_null();
        }
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        json_set(emit, "trace", ctx.trace);
        char *s = json_serialize(emit);
        if (s) {
            fputs(s, stdout);
            fputc('\n', stdout);
            free(s);
        }
        json_free(emit);
        json_free(ctx.counters);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}