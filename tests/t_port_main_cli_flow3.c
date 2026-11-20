/* t_port_main_cli_flow3.c — harness for the hermes_cli/main.py FINAL
 * UNCLAIMED-remainder FLOW port (src/cli/main_cli_flow3.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io":
 * {seam: script}, "env": {...}}}. Emits JSON LINES: {"func","in","out":
 * {"ret","trace","state"}}. "ret" is the canonical return (tuples -> arrays,
 * None -> null, uncaught exception -> {"__raise__": cls}); "trace" is the
 * ordered seam-call sequence [{"ev","args","ret"}]; "state" is {} (none of
 * the six targets mutates caller-visible state — env writes ride seams).
 * The oracle drives LIVE hermes_cli/main.py under the SAME scripts and
 * diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "cli/main_cli_flow3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *mc3_harness_run(mc3_ctx *ctx, const char *func, json_t *in) {
    if (strcmp(func, "_skills_dir_is_unseeded") == 0)
        return mc3_skills_dir_is_unseeded(ctx, in);
    if (strcmp(func, "_skills_sync_bg") == 0)
        return mc3_skills_sync_bg(ctx, in);
    if (strcmp(func, "_cmd") == 0)
        return mc3_forward_cmd(ctx, in);
    if (strcmp(func, "_custody_refusal") == 0)
        return mc3_custody_refusal(ctx, in);
    if (strcmp(func, "_build_cli_parser") == 0)
        return mc3_build_cli_parser(ctx, in);
    if (strcmp(func, "main") == 0)
        return mc3_main(ctx, in);
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 20];
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
        mc3_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *ret = mc3_harness_run(&ctx, func, in);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(mc3_exc_class(&ctx)));
            json_free(ctx.exc);
            json_free(ret);
        } else {
            ret_out = ret ? ret : json_null();
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