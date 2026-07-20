/*
 * t_port_file_mutation_verifier.c — oracle harness for the file-mutation
 * verifier (src/agent/file_mutation_verifier.c):
 *   file_mutation_tracker_init / _record / _format_footer
 * Port of run_agent.AIAgent._record_file_mutation_result +
 * _format_file_mutation_failure_footer.
 *
 * Fixture ops (one per line):
 *   init                       reset tracker
 *   record <tool> <path> <iserr> <errpreview>
 *                             record one mutation outcome (iserr 0/1)
 *   format                     emit current footer as JSON string
 *
 * Minimal includes: forward-declare symbols; no god header.
 */

#include "file_mutation_verifier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static file_mutation_tracker_t TR;

static void emit_json_string(const char *s) {
    if (!s) { printf("null"); return; }
    putchar('"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n"); break;
            case '\r': printf("\\r"); break;
            case '\t': printf("\\t"); break;
            default:
                if (c < 0x20) printf("\\u%04x", c);
                else putchar((int)c);
        }
    }
    putchar('"');
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[0]); return 2; }

    printf("{\"results\":[");
    int first = 1;
    char *line = NULL; size_t cap = 0; ssize_t n;
    while ((n = getline(&line, &cap, f)) != -1) {
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;

        if (strncmp(line, "init", 4) == 0 && (line[4] == '\0' || line[4] == ' ')) {
            file_mutation_tracker_init(&TR);
            continue;
        }
        if (strncmp(line, "format", 5) == 0) {
            char *footer = file_mutation_tracker_format_footer(&TR);
            if (!first) printf(",");
            first = 0;
            printf("{\"op\":\"format\",\"footer\":");
            emit_json_string(footer);
            printf("}");
            free(footer);
            continue;
        }
        if (strncmp(line, "record ", 7) == 0) {
            /* record <tool> <path> <iserr> <errpreview...> */
            char *p = line + 7;
            char *tool = p;
            char *sp = strchr(tool, ' ');
            if (!sp) continue;
            *sp = '\0'; p = sp + 1;
            char *path = p;
            sp = strchr(path, ' ');
            if (!sp) continue;
            *sp = '\0'; p = sp + 1;
            char *iserr_s = p;
            sp = strchr(iserr_s, ' ');
            char *preview = NULL;
            if (sp) { *sp = '\0'; preview = sp + 1; }
            int iserr = (iserr_s[0] == '1');
            file_mutation_tracker_record(&TR, tool, path, preview ? preview : "", iserr);
            continue;
        }
    }
    printf("]}");
    free(line);
    fclose(f);
    return 0;
}
