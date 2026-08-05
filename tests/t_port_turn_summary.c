/* t_port_turn_summary.c — differential oracle driver for agent/turn_summary.py. */

#include "port_turn_summary.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jb(bool v) { return v ? "true" : "false"; }
/* json-escape a string for the oracle line */
static void jstr(const char *s) {
    fputc('"', stdout);
    for (const char *p = s; p && *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default: fputc(*p, stdout);
        }
    }
    fputc('"', stdout);
}

static turn_tally_t *mk_tally(void) { return turn_tally_new(); }

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    /* --- format_elapsed --- */
    {
        char *a = turn_summary_format_elapsed(12.4);
        char *b = turn_summary_format_elapsed(125.0);
        char *c = turn_summary_format_elapsed(-3.0);
        printf("{\"case\":\"elapsed\",\"a\":%s,\"b\":%s,\"neg\":%s}\n",
               jb(strcmp(a, "12.4s") == 0), jb(strcmp(b, "2m05s") == 0),
               jb(strcmp(c, "0.0s") == 0));
        free(a); free(b); free(c);
    }

    /* --- _pluralize --- */
    {
        char *p1 = turn_summary_pluralize(1, "file");
        char *p2 = turn_summary_pluralize(3, "files");
        char *p3 = turn_summary_pluralize(1, "boxes");   /* -> box */
        char *p4 = turn_summary_pluralize(2, "pages");
        char *p5 = turn_summary_pluralize(1, "statuses"); /* -> status */
        printf("{\"case\":\"pluralize\",\"p1\":%s,\"p2\":%s,\"p3\":%s,\"p4\":%s,\"p5\":%s}\n",
               jb(strcmp(p1, "1 file") == 0), jb(strcmp(p2, "3 files") == 0),
               jb(strcmp(p3, "1 boxe") == 0), jb(strcmp(p4, "2 pages") == 0),
               jb(strcmp(p5, "1 status") == 0));
        free(p1); free(p2); free(p3); free(p4); free(p5);
    }

    /* --- count_diff_lines --- */
    {
        const char *diff = "--- a/x\n+++ b/x\n@@ -1,1 +1,2 @@\n-old\n+new\n+more\n";
        int ad, rm;
        turn_summary_count_diff_lines(diff, &ad, &rm);
        printf("{\"case\":\"diff\",\"added\":%d,\"removed\":%d}\n", ad, rm);
    }

    /* --- format_turn_summary (no tools, fast) --- */
    {
        turn_tally_t *t = mk_tally();
        char *s = turn_summary_format(1.0, t, TURN_SUMMARY_MAX_SEGMENTS);
        printf("{\"case\":\"empty_fast\",\"empty\":%s}\n", jb(strcmp(s, "") == 0));
        free(s); turn_tally_free(t);
    }

    /* --- collector: edits + reads + commands --- */
    {
        turn_summary_collector_t *col = turn_summary_collector_new();
        /* patch with a diff payload reporting +6 -2 */
        json_t *diff = json_object();
        json_set(diff, "diff", json_string("--- a\n+++ b\n@@ -1 +1,2 @@\n-a\n+b\n+c\n"));
        turn_summary_collector_record(col, "write_file", NULL, false);
        turn_summary_collector_record(col, "patch", diff, false);
        turn_summary_collector_record(col, "read_file", NULL, false);
        turn_summary_collector_record(col, "read_file", NULL, false);
        turn_summary_collector_record(col, "terminal", NULL, false);
        turn_summary_collector_record(col, "terminal", NULL, false);
        turn_summary_collector_record(col, "terminal", NULL, false);
        turn_summary_collector_record(col, "_thinking", NULL, false); /* skipped */
        turn_summary_collector_record(col, "unknown_tool", NULL, false); /* other */
        turn_summary_collector_record(col, "write_file", NULL, true); /* error skipped */
        int total = turn_tally_total_tools(turn_summary_collector_tally(col));
        char *s = turn_summary_collector_render(col, 12.4);
        printf("{\"case\":\"collector\",\"total\":%d,\"line\":%s}\n", total, jb(true));
        /* emit the rendered line so the oracle can compare exactly */
        printf("{\"case\":\"render_line\",\"line\":");
        jstr(s);
        printf("}\n");
        free(s);
        json_free(diff);
        turn_summary_collector_free(col);
    }

    /* --- format_turn_summary: many verbs -> segment cap --- */
    {
        turn_tally_t *t = mk_tally();
        /* build many distinct verbs by recording unknown-ish tools */
        /* use write_file, read_file, terminal, execute_code, search_files, web_search */
        const char *tools[] = {"write_file","read_file","terminal","execute_code","search_files","web_search"};
        turn_summary_collector_t *col = turn_summary_collector_new();
        for (int i = 0; i < 6; i++) turn_summary_collector_record(col, tools[i], NULL, false);
        char *s = turn_summary_collector_render(col, 30.0);
        printf("{\"case\":\"many_verbs\",\"line\":");
        jstr(s);
        printf("}\n");
        free(s);
        turn_summary_collector_free(col);
        turn_tally_free(t);
    }

    /* --- format_token_flow --- */
    {
        json_t *z = json_number(0);
        json_t *small = json_number(500);
        json_t *k = json_number(1200);
        json_t *M = json_number(2500000);
        char *a = turn_summary_format_token_flow(z, "↓");
        char *b = turn_summary_format_token_flow(small, "↓");
        char *c = turn_summary_format_token_flow(k, "↓");
        char *d = turn_summary_format_token_flow(M, "↓");
        printf("{\"case\":\"token_flow\",\"zero\":%s,\"small\":%s,\"k\":%s,\"M\":%s}\n",
               jb(strcmp(a, "") == 0), jb(strcmp(b, "↓ 500 tok") == 0),
               jb(strcmp(c, "↓ 1.2k tok") == 0), jb(strcmp(d, "↓ 2.5M tok") == 0));
        free(a); free(b); free(c); free(d);
        json_free(z); json_free(small); json_free(k); json_free(M);
    }

    return 0;
}
