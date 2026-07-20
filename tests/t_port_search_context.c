/*
 * t_port_search_context.c — oracle harness for the PURE search-context-line
 * parser in src/tools/file_text_ops.c (port of
 * tools/file_operations.py:_parse_search_context_line). Deterministic string
 * parse: rightmost "-<digits>-" separator -> [path, line, content], else null.
 */

#include "tools/file_text_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_string(const char *s) {
    if (!s) { printf("null"); return; }
    putchar('"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n"); break;
            default:
                if (c < 0x20) printf("\\u%04x", c);
                else putchar((int)c);
        }
    }
    putchar('"');
}

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        char *res = file_text_ops_parse_search_context_line(line[0] ? line : "");
        printf("{\"in\":");
        emit_json_string(line[0] ? line : "");
        printf(",\"out\":");
        /* res is a JSON array ["path",N,"content"], or the literal "null"
         * (quoted) for the Python-None case. Emit unquoted null to match the
         * oracle's JSON null. */
        if (res && strcmp(res, "\"null\"") == 0) printf("null");
        else fputs(res ? res : "null", stdout);
        printf("}\n");
        free(res);
    }
    return 0;
}
