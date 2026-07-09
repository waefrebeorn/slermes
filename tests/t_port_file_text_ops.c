/*
 * t_port_file_text_ops.c — oracle harness for the extracted file_text_ops
 * module (v551). Calls each public fn over a fixture set and emits one JSON
 * line per (fn, input): {"fn":...,"in":...,"out":...}. A companion Python
 * script proves C == LIVE tools/file_operations.py.
 *
 * Build:
 *   gcc -O2 -I include -I src/tools -I lib/libjson \
 *       tests/t_port_file_text_ops.c src/tools/file_text_ops.o \
 *       lib/libjson/json.o -o /tmp/t_fto
 */
#include "file_text_ops.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* Minimal hermes_log stub */
void hermes_log(int level, const char *module, const char *fmt, ...)
{ (void)level; (void)module; (void)fmt; }

static void emit_str(const char *fn, const char *in, const char *out)
{
    printf("{\"fn\":\"%s\",\"in\":\"", fn);
    const char *p = in ? in : "";
    while (*p) {
        unsigned char c = (unsigned char)*p++;
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
    printf("\",\"out\":\"");
    const char *q = out ? out : "";
    while (*q) {
        unsigned char c = (unsigned char)*q++;
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
    printf("\"}\n");
}

static void emit_json(const char *fn, const char *in, const char *out)
{
    printf("{\"fn\":\"%s\",\"in\":\"", fn);
    const char *p = in ? in : "";
    while (*p) { unsigned char c=(unsigned char)*p++; if(c=='"'||c=='\\'){putchar('\\');putchar(c);} else if(c<0x20) printf("\\u%04x",c); else putchar(c); }
    printf("\",\"out\":%s}\n", out ? out : "{}");
}

int main(void)
{
    /* strip_terminal_fence_leaks */
    const char *ansi = "\033[31mred\033[0m";
    const char *osc = "\033]0;title\007";
    const char *fence = "__HERMES_FENCE_abc__\nsome line\n";
    emit_str("strip_terminal_fence_leaks", ansi, file_text_ops_strip_terminal_fence_leaks(ansi));
    emit_str("strip_terminal_fence_leaks", osc, file_text_ops_strip_terminal_fence_leaks(osc));
    emit_str("strip_terminal_fence_leaks", fence, file_text_ops_strip_terminal_fence_leaks(fence));

    /* detect_line_ending */
    emit_str("detect_line_ending", "a\r\nb", file_text_ops_detect_line_ending("a\r\nb"));
    emit_str("detect_line_ending", "a\nb", file_text_ops_detect_line_ending("a\nb"));
    emit_str("detect_line_ending", "a\rb", file_text_ops_detect_line_ending("a\rb"));
    emit_str("detect_line_ending", "noeol", file_text_ops_detect_line_ending("noeol"));

    /* normalize_line_endings */
    emit_str("normalize_line_endings", "a\r\nb\r\nc", file_text_ops_normalize_line_endings("a\r\nb\r\nc", "\n"));
    emit_str("normalize_line_endings", "a\rb\rc", file_text_ops_normalize_line_endings("a\rb\rc", "\r\n"));
    emit_str("normalize_line_endings", "a\nb", file_text_ops_normalize_line_endings("a\nb", "\r\n"));

    /* strip_bom / has_bom */
    emit_str("strip_bom", "\xEF\xBB\xBFhello", file_text_ops_strip_bom("\xEF\xBB\xBFhello"));
    emit_str("strip_bom", "hello", file_text_ops_strip_bom("hello"));
    emit_str("has_bom", "\xEF\xBB\xBFx", file_text_ops_has_bom("\xEF\xBB\xBFx") ? "1" : "0");
    emit_str("has_bom", "x", file_text_ops_has_bom("x") ? "1" : "0");

    /* add_line_numbers (max_line_length=2000 to mirror Python get_max_line_length) */
    emit_str("add_line_numbers", "a\nb\nc", file_text_ops_add_line_numbers("a\nb\nc", 1, 2000));
    emit_str("add_line_numbers", "", file_text_ops_add_line_numbers("", 1, 2000));
    char longline[200];
    for (int i = 0; i < 199; i++) longline[i] = 'x'; longline[199] = '\0';
    char *ln = file_text_ops_add_line_numbers(longline, 1, 2000);
    emit_str("add_line_numbers_trunc", longline, ln);
    free(ln);

    /* expand_path: shells out to $HOME in Python; C uses $HOME env var.
     * Not oracle-verified (backend-dependent) — kept as a smoke call. */
    char *ep = file_text_ops_expand_path("~/.config");
    emit_str("expand_path", "~/.config", ep);
    free(ep);

    /* escape_shell_arg */
    emit_str("escape_shell_arg", "plain", file_text_ops_escape_shell_arg("plain"));
    emit_str("escape_shell_arg", "it's", file_text_ops_escape_shell_arg("it's"));
    emit_str("escape_shell_arg", NULL, file_text_ops_escape_shell_arg(NULL));

    /* parse_search_context_line (grep/rg path-line-content) */
    char *pcl = file_text_ops_parse_search_context_line("src/foo.c-12-context here");
    emit_json("parse_search_context_line", "src/foo.c-12-context here", pcl);
    free(pcl);
    char *pcl2 = file_text_ops_parse_search_context_line("src/foo.c:12: bar");
    emit_json("parse_search_context_line_none", "src/foo.c:12: bar", pcl2);
    free(pcl2);

    return 0;
}
