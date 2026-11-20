/* t_port_lifecycle_guard_flow2.c — harness for the cron/lifecycle_guard.py
 * UNCLAIMED-remainder FLOW port (src/cron/lifecycle_guard_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}, "env": {...}}}. Emits JSON LINES: {"func","in","out":{"ret","trace",
 * "state"}}. "ret" is the canonical return (None -> null, tuple -> array, Path
 * -> string, uncaught exception -> {"__raise__":cls,"msg":...}); "trace" is the
 * ordered seam-call sequence [{"ev","args","ret"}]; "state" echoes the mutated
 * budget/written carrier where applicable ({} otherwise, and {} on a raise).
 * The oracle drives the LIVE module under the SAME scripts and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson + pcre2, both
 * supplied by run_one_oracle.sh).
 */
#include "hermes_json.h"
#include "cron/lifecycle_guard_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 17];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) { fprintf(stderr, "harness: unparseable case line\n"); return 1; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *empty_in = NULL;
        if (!in) { empty_in = json_object(); in = empty_in; }
        lg2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = lg2_dispatch(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_copy(ctx.exc);
            json_free(ctx.exc); ctx.exc = NULL;
            json_free(ret); ret = NULL;
            json_free(state); state = NULL;
        } else {
            ret_out = ret ? ret : json_null();
            ret = NULL;
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        ctx.trace = NULL;
        json_set(out, "state", state ? state : json_object());
        state = NULL;
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) { fputs(s, stdout); fputc('\n', stdout); free(s); }
        json_free(emit);
        json_free(ctx.trace);
        json_free(ctx.counters);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}
