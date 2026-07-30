/*
 * t_port_file_pagination_ops.c — oracle harness for v554 file_pagination_ops
 * extraction. Emits JSON lines consumed by sta_oracle_file_pagination_ops.py.
 *
 *   gcc -O2 -g -I include -I src/tools -I src/agent -I lib/libjson \
 *       tests/t_port_file_pagination_ops.c src/tools/file_pagination_ops.o \
 *       lib/libjson/json.o -o /tmp/t_pg
 */
#include "file_pagination_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_escaped(const char *s)
{
    if (!s) { printf("null"); return; }
    putchar('"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}

static void emit_json(const char *fn, const char *in, const char *out)
{
    printf("{\"fn\":\"%s\"", fn);
    if (in) { printf(",\"in\":"); emit_escaped(in); }
    printf(",\"out\":"); emit_escaped(out);
    printf("}\n");
}

int main(void)
{
    /* normalize_read_pagination -> offset/limit JSON */
    char *r;
    r = file_pagination_ops_normalize_read_pagination(1, 500, 500);
    emit_json("read_pagination", "1|500|500", r); free(r);
    r = file_pagination_ops_normalize_read_pagination(0, 0, 500);   /* offset->1, limit->500 */
    emit_json("read_pagination", "0|0|500", r); free(r);
    r = file_pagination_ops_normalize_read_pagination(5, 100000, 500); /* limit capped to 2000 */
    emit_json("read_pagination", "5|100000|500", r); free(r);
    r = file_pagination_ops_normalize_read_pagination(-3, 10, 500); /* offset->1 */
    emit_json("read_pagination", "-3|10|500", r); free(r);
    r = file_pagination_ops_normalize_read_pagination(2, -5, 500);  /* limit<=0 -> default 500 */
    emit_json("read_pagination", "2|-5|500", r); free(r);

    /* normalize_search_pagination -> offset>=0, limit>=1 */
    r = file_pagination_ops_normalize_search_pagination(0, 50, 50);
    emit_json("search_pagination", "0|50|50", r); free(r);
    r = file_pagination_ops_normalize_search_pagination(-2, 0, 50); /* offset->0, limit->50 */
    emit_json("search_pagination", "-2|0|50", r); free(r);
    r = file_pagination_ops_normalize_search_pagination(3, 100, 50);
    emit_json("search_pagination", "3|100|50", r); free(r);

    /* is_line_oriented_newline_error */
    emit_json("is_line_oriented_error",
        "literal \"\\n\" is not allowed in --multiline mode: use -U",
        file_pagination_ops_is_line_oriented_newline_error(
            "literal \"\\n\" is not allowed in --multiline mode: use -U") ? "true" : "false");
    emit_json("is_line_oriented_error",
        "regular error about file not found",
        file_pagination_ops_is_line_oriented_newline_error("regular error about file not found") ? "true" : "false");
    emit_json("is_line_oriented_error", "",
        file_pagination_ops_is_line_oriented_newline_error("") ? "true" : "false");
    emit_json("is_line_oriented_error", NULL,
        file_pagination_ops_is_line_oriented_newline_error(NULL) ? "true" : "false");
    /* Python: needs BOTH substrings; the old C matched "newline" alone -> false positive */
    emit_json("is_line_oriented_error",
        "newline error detected here",
        file_pagination_ops_is_line_oriented_newline_error("newline error detected here") ? "true" : "false");

    /* pattern_has_regex_newline */
    emit_json("pattern_has_regex_newline", "foo\\nbar",
        file_pagination_ops_pattern_has_regex_newline("foo\\nbar") ? "true" : "false");
    emit_json("pattern_has_regex_newline", "foo\\\\nbar", /* even backslash => literal */
        file_pagination_ops_pattern_has_regex_newline("foo\\\\nbar") ? "true" : "false");
    emit_json("pattern_has_regex_newline", "foo\\\\\\nbar", /* odd => escape */
        file_pagination_ops_pattern_has_regex_newline("foo\\\\\\nbar") ? "true" : "false");
    emit_json("pattern_has_regex_newline", "^start.*end$",
        file_pagination_ops_pattern_has_regex_newline("^start.*end$") ? "true" : "false");
    emit_json("pattern_has_regex_newline", "plain text",
        file_pagination_ops_pattern_has_regex_newline("plain text") ? "true" : "false");

    /* maybe_warn_line_oriented_newline_pattern
     * Build a result JSON {total_count, error}; emit serialized after call. */
    json_t *res;
    char *out;
    /* Case A: total_count=0, pattern has regex \n, error is line-oriented -> WARN */
    res = json_object();
    json_set(res, "total_count", json_number(0));
    json_set(res, "error", json_string("literal \"\\n\" is not allowed in --multiline mode: use -U"));
    res = file_pagination_ops_maybe_warn_line_oriented_newline_pattern(res, "a\\nb");
    out = json_serialize(res); json_free(res);
    emit_json("maybe_warn", "tc=0|lineerr|a\\nb", out); free(out);

    /* Case B: total_count>0 -> no warn */
    res = json_object();
    json_set(res, "total_count", json_number(3));
    json_set(res, "error", json_string("literal \"\\n\" is not allowed in --multiline mode: use -U"));
    res = file_pagination_ops_maybe_warn_line_oriented_newline_pattern(res, "a\\nb");
    out = json_serialize(res); json_free(res);
    emit_json("maybe_warn", "tc=3|lineerr|a\\nb", out); free(out);

    /* Case C: total_count=0 but pattern has NO regex newline -> no warn */
    res = json_object();
    json_set(res, "total_count", json_number(0));
    json_set(res, "error", json_string("literal \"\\n\" is not allowed in --multiline mode: use -U"));
    res = file_pagination_ops_maybe_warn_line_oriented_newline_pattern(res, "plain");
    out = json_serialize(res); json_free(res);
    emit_json("maybe_warn", "tc=0|lineerr|plain", out); free(out);

    /* Case D: total_count=0, regex \n, but error is a DIFFERENT error -> no warn */
    res = json_object();
    json_set(res, "total_count", json_number(0));
    json_set(res, "error", json_string("some other error"));
    res = file_pagination_ops_maybe_warn_line_oriented_newline_pattern(res, "a\\nb");
    out = json_serialize(res); json_free(res);
    emit_json("maybe_warn", "tc=0|othererr|a\\nb", out); free(out);

    return 0;
}
