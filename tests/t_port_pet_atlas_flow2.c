/* t_port_pet_atlas_flow2.c — harness for the UNCLAIMED-remainder FLOW port
 * of agent/pet/generate/atlas.py (src/agent/pet/generate/pet_atlas_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io":
 * {seam: script}}}. Emits JSON LINES: {"func","in","out":{"ret","trace",
 * "state"}}. "ret" is the canonical return; "trace" is the ordered seam
 * sequence [{"ev","args","ret"}]; "state" is {} for these read-only flows.
 * An uncaught exception -> {"__raise__": cls}. The oracle drives LIVE
 * agent/pet/generate/atlas.py defs under the same scripts and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/pet/generate/pet_atlas_flow2.c"

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

        pa2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.state = json_object();
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;

        json_t *ret = pa2_dispatch(&ctx, func, in);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(pa2_exc_class(&ctx)));
            json_free(ctx.exc); ctx.exc = NULL;
            json_free(ret); ret = NULL;
        } else {
            ret_out = ret ? ret : json_null();
            ret = NULL;
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", json_object());
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) { fputs(s, stdout); fputc('\n', stdout); free(s); }
        json_free(emit);
        json_free(ctx.counters);
        json_free(ctx.state);
        json_free(ret);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}
