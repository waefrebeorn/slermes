/* t_port_plugins_install_flow2.c — harness for the UNCLAIMED FLOW port
 * src/cli/plugins_install_flow2.c of hermes_cli/plugins_install_scan.py.
 *
 * Reads JSON LINES {"func":<name>,"in":{...}} from stdin; emits JSON LINES
 * {"func","in","out":{"ret","trace","state"}}. "ret" is the canonical return
 * (None -> null, uncaught exception -> {"__raise__":cls}). The oracle drives
 * the LIVE quarry defs under the same inputs and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "cli/plugins_install_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *pi2_run(pi2_ctx *ctx, const char *func, json_t *in) {
    if (!strcmp(func, "_walk"))                      return pi2_walk(ctx, in);
    if (!strcmp(func, "_finding"))                   return pi2_finding(ctx, in);
    if (!strcmp(func, "_is_main_guard"))             return pi2_is_main_guard(ctx, in);
    if (!strcmp(func, "_parse_python_source"))       return pi2_parse_python_source(ctx, in);
    if (!strcmp(func, "_main_guard_body_lines"))     return pi2_main_guard_body_lines(ctx, in);
    if (!strcmp(func, "_declares_non_utf8_source"))  return pi2_declares_non_utf8_source(ctx, in);
    if (!strcmp(func, "_docstring_lines"))           return pi2_docstring_lines(ctx, in);
    if (!strcmp(func, "_docstring_cap"))             return pi2_docstring_cap(ctx, in);
    if (!strcmp(func, "_filter_findings"))           return pi2_filter_findings(ctx, in);
    if (!strcmp(func, "_at_most"))                   return pi2_at_most(ctx, in);
    if (!strcmp(func, "_comment_severity"))          return pi2_comment_severity(ctx, in);
    if (!strcmp(func, "_file_lines"))                return pi2_file_lines(ctx, in);
    if (!strcmp(func, "_context_severity"))          return pi2_context_severity(ctx, in);
    if (!strcmp(func, "_inert_context_cap"))         return pi2_inert_context_cap(ctx, in);
    if (!strcmp(func, "_is_defensive_documentation"))return pi2_is_defensive_documentation(ctx, in);
    if (!strcmp(func, "_dangerous_findings_summary"))return pi2_dangerous_findings_summary(ctx, in);
    if (!strcmp(func, "_tracked_bytecode"))          return pi2_tracked_bytecode(ctx, in);
    if (!strcmp(func, "_bytecode_findings"))         return pi2_bytecode_findings(ctx, in);
    if (!strcmp(func, "_check_plugin_structure"))    return pi2_check_plugin_structure(ctx, in);
    if (!strcmp(func, "scan_plugin"))                return pi2_scan_plugin(ctx, in);
    if (!strcmp(func, "should_allow_plugin_install"))return pi2_should_allow_plugin_install(ctx, in);
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 18];
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
        pi2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.pos = json_object();
        ctx.exc = NULL;
        json_t *ret = pi2_run(&ctx, func, in);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(json_get_str(ctx.exc, "__raise__", "Exception")));
            json_free(ctx.exc);
            if (ret) json_free(ret);
        } else {
            ret_out = ret ? ret : json_null();
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", json_array());
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
        json_free(ctx.pos);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}
