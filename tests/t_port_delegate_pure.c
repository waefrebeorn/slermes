/*
 * t_port_delegate_pure.c — oracle harness for the PURE delegate_tool.py helpers
 * in src/tools/delegate.c: delegate_stringify_tool_content,
 * delegate_looks_like_error_output, delegate_normalize_role.
 *
 * Deterministic, no I/O / network. Builds JSON content nodes from the fixture
 * and calls the REAL C functions, emitting stable JSON (one object per line)
 * that the Python oracle reproduces.
 */

#include "delegate_pure.h"
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
            case '\t': printf("\\t"); break;
            case '\r': printf("\\r"); break;
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
    char line[16384];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        char op[40];
        const char *rest;
        split_kv(line, op, sizeof(op), &rest);
        const char *v = rest[0] ? rest : "";

        if (strcmp(op, "stringify") == 0 || strcmp(op, "error") == 0) {
            json_node_t *node = json_parse(v, NULL);
            if (strcmp(op, "stringify") == 0) {
                char buf[16384];
                delegate_stringify_tool_content(node, buf, sizeof(buf));
                printf("{\"op\":\"stringify\",\"in\":");
                emit_json_string(v); printf(",\"out\":");
                emit_json_string(buf); printf("}\n");
            } else {
                bool err = delegate_looks_like_error_output(node);
                printf("{\"op\":\"error\",\"in\":");
                emit_json_string(v); printf(",\"is_error\":%s}\n", err ? "true" : "false");
            }
            if (node) json_free(node);

        } else if (strcmp(op, "role") == 0) {
            char out[64];
            delegate_normalize_role(v, out, sizeof(out));
            printf("{\"op\":\"role\",\"in\":");
            emit_json_string(v); printf(",\"out\":");
            emit_json_string(out); printf("}\n");

        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
