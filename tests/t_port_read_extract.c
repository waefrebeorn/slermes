/*
 * t_port_read_extract.c — oracle harness for the PURE document-type helpers in
 * src/tools/port_tools_read_extract.c (ports of tools/read_extract.py:
 * _extension, is_extractable_document). Deterministic extension classification.
 */

#include "read_extract.h"
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
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        /* op 'ext <path>' */
        if (strncmp(line, "ext ", 4) == 0) {
            const char *v = line + 4;
            const char *ext = read_extract_extension(v);
            int ex = read_extract_is_extractable_document(v);
            printf("{\"op\":\"ext\",\"in\":");
            emit_json_string(v);
            printf(",\"ext\":");
            emit_json_string(ext ? ext : "");
            printf(",\"extractable\":%s}\n", ex ? "true" : "false");
        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
