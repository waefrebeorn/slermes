/* t_port_acp_tools_flow2.c — fixture-driven FLOW oracle harness for the C11
 * port of acp_adapter/tools.py (src/acp_adapter/acp_tools_flow2.c).
 *
 * Reads JSONL cases from stdin: {"func":<live_name>,"in":{"argv":[..],"kw":{..},
 * "seams":{name: const | [queue]}}}. Emits one JSON line per case:
 *   {"func":..,"ret":<value|raise spec>,"trace":[{"ev","args","ret"}],"st":{}}
 * The port file (included by bare name) implements every func as static at2_*.
 *
 * Gate: bash tests/run_one_oracle.sh acp_tools_flow2 \
 *           oracle_acp_tools_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "acp_adapter/acp_tools_flow2.c"

typedef json_t *(*at2_fn)(at2_io_t *, const json_t *, const json_t *);
typedef struct { const char *name; at2_fn fn; } at2_dispatch_t;

static const at2_dispatch_t AT2_DISPATCH[] = {
    { "get_tool_kind", at2_get_tool_kind },
    { "make_tool_call_id", at2_make_tool_call_id },
    { "_text", at2__text },
    { "_arg", at2__arg },
    { "_first", at2__first },
    { "_fmt", at2__fmt },
    { "_nonempty", at2__nonempty },
    { "_plural", at2__plural },
    { "_failure", at2__failure },
    { "coerce_tool_args", at2_coerce_tool_args },
    { "_args_json", at2__args_json },
    { "_json_loads_maybe", at2__json_loads_maybe },
    { "_truncate_text", at2__truncate_text },
    { "_fenced_text", at2__fenced_text },
    { "_tool_result_failed", at2__tool_result_failed },
    { "build_tool_title", at2_build_tool_title },
    { "_format_todo_result", at2_fmt_todo },
    { "_format_read_file_result", at2_fmt_read_file },
    { "_format_search_files_result", at2_fmt_search },
    { "_format_execute_code_result", at2_fmt_execute },
    { "_format_skill_view_result", at2_fmt_skill_view },
    { "_format_skill_manage_result", at2_fmt_skill_manage },
    { "_format_web_search_result", at2_fmt_web_search },
    { "_format_web_extract_result", at2_fmt_web_extract },
    { "_format_process_result", at2_fmt_process },
    { "_format_delegate_result", at2_fmt_delegate },
    { "_format_session_search_result", at2_fmt_session_search },
    { "_format_memory_result", at2_fmt_memory },
    { "_format_edit_result", at2_fmt_edit },
    { "_format_browser_result", at2_fmt_browser },
    { "_format_media_or_cron_result", at2_fmt_media },
    { "_format_structured_value", at2__format_structured_value },
    { "_format_generic_structured_result", at2__format_generic_structured_result },
    { "_parse_unified_diff_content", at2__parse_unified_diff_content },
    { "_build_tool_complete_content", at2__build_tool_complete_content },
    { "_more", at2__more },
    { "_start_todo", at2__start_todo },
    { "_start_skill_manage", at2__start_skill_manage },
    { "_start_execute_code", at2__start_execute_code },
    { "_start_delegate", at2__start_delegate },
    { "_preview", at2__preview },
    { "build_tool_start", at2_build_tool_start },
    { "_build_tool_start", at2__build_tool_start },
    { "build_tool_complete", at2_build_tool_complete },
    { "build_tool_abandoned", at2_build_tool_abandoned },
    { "extract_locations", at2_extract_locations },
    { NULL, NULL }
};

static char *at2h_readline(FILE *f)
{
    size_t cap = 8192, n = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') break;
        if (n + 1 >= cap) { cap *= 2; char *nb = (char *)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
        buf[n++] = (char)c;
    }
    if (n == 0 && c == EOF) { free(buf); return NULL; }
    buf[n] = 0;
    while (n > 0 && buf[n - 1] == '\r') buf[--n] = 0; /* CRLF-aware */
    return buf;
}

int main(int argc, char **argv)
{
    FILE *f = stdin;
    if (argc > 1) { f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; } }
    char *line;
    while ((line = at2h_readline(f)) != NULL) {
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        free(line);
        if (!rec || !json_is_object(rec)) { if (rec) json_free(rec); free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *argv_node = in ? json_obj_get(in, "argv") : NULL;
        json_t *kw_node = in ? json_obj_get(in, "kw") : NULL;
        json_t *seams_node = in ? json_obj_get(in, "seams") : NULL;
        if (!json_is_array(argv_node)) argv_node = json_array();
        if (!json_is_object(kw_node)) kw_node = json_object();
        if (!json_is_object(seams_node)) seams_node = json_object();

        at2_fn fn = NULL;
        for (int i = 0; AT2_DISPATCH[i].name; i++)
            if (strcmp(func, AT2_DISPATCH[i].name) == 0) { fn = AT2_DISPATCH[i].fn; break; }

        at2_io_t io;
        io.seams = seams_node;
        io.cursors = json_object();
        io.trace = json_array();
        json_t *ret;
        if (fn) ret = fn(&io, argv_node, kw_node);
        else ret = at2_mkraise("RuntimeError", "unknown func");

        json_t *out = json_object();
        json_set(out, "func", json_string(func));
        json_set(out, "in", in ? json_copy(in) : json_object());
        json_set(out, "ret", ret ? json_copy(ret) : json_null());
        json_set(out, "trace", io.trace);
        json_set(out, "st", json_object());
        char *serialized = json_serialize(out);
        fputs(serialized ? serialized : "{}\n", stdout);
        fputc('\n', stdout);
        free(serialized);
        json_free(out);
        json_free(ret);
        json_free(io.cursors);
        json_free(rec);
        free(err);
    }
    if (f != stdin) fclose(f);
    return 0;
}
