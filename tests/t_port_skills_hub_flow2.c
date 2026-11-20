/* t_port_skills_hub_flow2.c — harness for the hermes_cli/skills_hub.py
 * UNCLAIMED-remainder FLOW port (src/cli/skills_hub_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}. Emits JSON LINES:
 * {"func","in","out":{"ret","trace","state"}}. "ret" is the canonical return
 * (tuples -> arrays, None -> null, uncaught exception -> {"__raise__": cls});
 * "trace" is the ordered seam-call sequence (empty for this pure-helper wave);
 * "state" is {} (no mutable state is carried by these helpers). The oracle
 * drives LIVE hermes_cli/skills_hub.py under the same inputs and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "cli/skills_hub_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *sh2_harness_run(sh2_ctx *ctx, const char *func, json_t *in) {
    if (strcmp(func, "_trust_cell") == 0)               return sh2_trust_cell(ctx, in);
    if (strcmp(func, "_truncate") == 0)                  return sh2_truncate(ctx, in);
    if (strcmp(func, "_ident_col") == 0)                 return sh2_ident_col(ctx, in);
    if (strcmp(func, "_row") == 0)                       return sh2_row(ctx, in);
    if (strcmp(func, "_slug_key") == 0)                  return sh2_slug_key(ctx, in);
    if (strcmp(func, "_identifier_slug") == 0)           return sh2_identifier_slug(ctx, in);
    if (strcmp(func, "_exact_name_hits") == 0)           return sh2_exact_name_hits(ctx, in);
    if (strcmp(func, "_hub_rate_limited") == 0)          return sh2_hub_rate_limited(ctx, in);
    if (strcmp(func, "_hub_rejected_credentials") == 0)  return sh2_hub_rejected_credentials(ctx, in);
    if (strcmp(func, "_upstream_repo") == 0)             return sh2_upstream_repo(ctx, in);
    if (strcmp(func, "_opt_value") == 0)                 return sh2_opt_value(ctx, in);
    if (strcmp(func, "_int_or") == 0)                    return sh2_int_or(ctx, in);
    if (strcmp(func, "_opt_int") == 0)                   return sh2_opt_int(ctx, in);
    if (strcmp(func, "_first_positional") == 0)          return sh2_first_positional(ctx, in);
    return NULL;
}

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
        sh2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *ret = sh2_harness_run(&ctx, func, in);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(sh2_exc_class(&ctx)));
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
