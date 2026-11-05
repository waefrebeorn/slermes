/*
 * t_port_turn_summary.c — Oracle harness for agent/turn_summary.py
 * (ported to src/cli/port_turn_summary.c).
 */
#include "port_turn_summary.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_str(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

struct step { const char *tool, *result; int is_error; };

static void emit_summary(const char *name, double elapsed,
                         const struct step *steps, int n, int max_segments)
{
    ts_tally_t tally;
    ts_tally_init(&tally);
    for (int i = 0; i < n; i++)
        ts_tally_record(&tally, steps[i].tool, steps[i].result, steps[i].is_error);

    char *out = ts_format_summary(elapsed, &tally, max_segments);
    printf("{\"family\":\"summary\",\"case\":\"%s\",\"elapsed\":%.17g,"
           "\"max_segments\":%d,\"steps\":[", name, elapsed, max_segments);
    for (int i = 0; i < n; i++) {
        if (i) fputs(",", stdout);
        fputs("{\"tool\":", stdout);
        emit_json_str(steps[i].tool);
        printf(",\"result\":");
        emit_json_str(steps[i].result);
        printf(",\"is_error\":%s}", steps[i].is_error ? "true" : "false");
    }
    printf("],\"out\":");
    emit_json_str(out);
    printf("}\n");
    free(out);
    ts_tally_clear(&tally);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* format_elapsed */
    {
        static const double vals[] = { 0.0, -3.0, 0.04, 1.25, 12.44, 59.9,
                                       60.0, 90.0, 125.4, 3599.0, 3600.0 };
        for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
            char *s = ts_format_elapsed(vals[i]);
            printf("{\"family\":\"elapsed\",\"arg\":%.17g,\"out\":", vals[i]);
            emit_json_str(s);
            printf("}\n");
            free(s);
        }
    }

    /* _pluralize */
    {
        static const struct { int n; const char *noun; } cases[] = {
            { 1, "files" }, { 2, "files" }, { 1, "pages" }, { 1, "commands" },
            { 1, "memories" }, { 1, "task lists" }, { 1, "paths" }, { 1, "times" },
            { 1, "skills" }, { 0, "files" }, { 1, "boxes" }, { 1, "scripts" },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char *s = ts_pluralize(cases[i].n, cases[i].noun);
            printf("{\"family\":\"plural\",\"n\":%d,\"noun\":", cases[i].n);
            emit_json_str(cases[i].noun);
            printf(",\"out\":");
            emit_json_str(s);
            printf("}\n");
            free(s);
        }
    }

    /* format_token_flow */
    {
        static const long vals[] = { 0, -5, 1, 999, 1000, 1234, 12345, 999999,
                                     1000000, 1250000 };
        for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
            char *s = ts_format_token_flow(vals[i], NULL);
            printf("{\"family\":\"tokens\",\"n\":%ld,\"out\":", vals[i]);
            emit_json_str(s);
            printf("}\n");
            free(s);
        }
    }

    /* summaries */
    emit_summary("none_fast", 0.5, NULL, 0, 4);
    emit_summary("none_slow", 3.0, NULL, 0, 4);
    {
        struct step s[] = { { "read_file", NULL, 0 }, { "read_file", NULL, 0 },
                            { "terminal", NULL, 0 } };
        emit_summary("reads_and_run", 12.4, s, 3, 4);
    }
    {
        struct step s[] = { { "write_file", NULL, 0 }, { "patch", NULL, 0 } };
        emit_summary("edits_no_delta", 5.2, s, 2, 4);
    }
    {
        struct step s[] = {
            { "patch", "{\"success\":true,\"diff\":\"--- a/x\\n+++ b/x\\n@@ -1 +1,2 @@\\n-old\\n+new\\n+extra\"}", 0 },
            { "patch", "{\"diff\":\"+++ b/y\\n--- a/y\\n+one\"}", 0 },
        };
        emit_summary("edits_with_delta", 18.0, s, 2, 4);
    }
    {
        struct step s[] = {
            { "patch", "{\"diff\":\"--- a/x\\n+++ b/x\"}", 0 },
        };
        emit_summary("edit_headers_only", 4.0, s, 1, 4);
    }
    {
        struct step s[] = { { "read_file", NULL, 0 }, { "read_file", NULL, 0 },
                            { "web_search", NULL, 0 }, { "todo_list", NULL, 0 },
                            { "memory", NULL, 0 } };
        emit_summary("collapse_more", 40.0, s, 5, 4);
    }
    {
        struct step s[] = { { "read_file", NULL, 0 }, { "read_file", NULL, 0 },
                            { "web_search", NULL, 0 }, { "todo_list", NULL, 0 },
                            { "memory", NULL, 0 } };
        emit_summary("no_collapse", 40.0, s, 5, 0);
    }
    {
        struct step s[] = { { "_thinking", NULL, 0 }, { "read_file", NULL, 1 },
                            { "some_plugin_tool", NULL, 0 },
                            { "another_plugin", NULL, 0 } };
        emit_summary("skipped_and_others", 8.0, s, 4, 4);
    }
    {
        struct step s[] = { { "read_file", NULL, 0 }, { "write_file", NULL, 0 },
                            { "terminal", NULL, 0 }, { "search_files", NULL, 0 } };
        emit_summary("ordering", 33.3, s, 4, 4);
    }
    return 0;
}
