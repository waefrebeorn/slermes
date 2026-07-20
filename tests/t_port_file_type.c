/*
 * t_port_file_type.c — oracle harness for the PURE file-type detection
 * helpers in src/tools/file_fs_ops.c (ports of tools/file_operations.py
 * _is_image / _is_likely_binary): file_fs_ops_is_image,
 * file_fs_ops_is_likely_binary. Deterministic extension-based checks
 * (extension-only; the C side has no content-sample path).
 */

#include "tools/file_fs_ops.h"
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

static void split_kv(const char *line, char *key, size_t ksz, const char **val) {
    size_t i = 0;
    while (*line && *line != ' ' && i + 1 < ksz) key[i++] = *line++;
    key[i] = '\0';
    if (*line == ' ') line++;
    *val = line;
}

int main(void) {
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        char op[40];
        const char *rest;
        split_kv(line, op, sizeof(op), &rest);
        const char *v = rest[0] ? rest : "";

        if (strcmp(op, "image") == 0) {
            bool r = file_fs_ops_is_image(v);
            printf("{\"op\":\"image\",\"in\":");
            emit_json_string(v);
            printf(",\"out\":%s}\n", r ? "true" : "false");
        } else if (strcmp(op, "binary") == 0) {
            bool r = file_fs_ops_is_likely_binary(v);
            printf("{\"op\":\"binary\",\"in\":");
            emit_json_string(v);
            printf(",\"out\":%s}\n", r ? "true" : "false");
        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
