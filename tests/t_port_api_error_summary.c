/*
 * t_port_api_error_summary.c — oracle harness for summarize_api_error()
 * (port of run_agent.AIAgent._summarize_api_error, src/agent/api_error_summary.c)
 *
 * Faithfulness boundary proven here: the string-input branches the C port
 * mirrors — malformed streaming, Cloudflare/HTML title (Ray ID + status),
 * JSON {"error":{...}} body, JSON {"error": "..."} string, and the raw
 * truncate fallback. Both sides receive the SAME raw string; the Python
 * SDK-exception-object branches (.body/.response) are out of scope for a
 * string-in harness and are not claimed by the C port.
 *
 * Minimal includes: forward-declare the symbol under test; no god header.
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Under test (linked via the full slermes object set). */
char *summarize_api_error(const char *raw_error);

/* Compact JSON string emit (handles escapes + control chars). */
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
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;

    printf("{\"results\":[");
    int first = 1;
    while ((n = getline(&line, &cap, f)) != -1) {
        /* strip trailing newline */
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;

        if (!first) printf(",");
        first = 0;

        char *summary = summarize_api_error(line);
        printf("{\"input\":");
        emit_json_string(line);
        printf(",\"summary\":");
        emit_json_string(summary);
        printf("}");
        free(summary);
    }
    printf("]}");

    free(line);
    fclose(f);
    return 0;
}
