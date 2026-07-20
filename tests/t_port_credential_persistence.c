/*
 * t_port_credential_persistence.c — oracle harness for the PURE credential-key
 * sanitization helpers in src/agent/credential_persistence.c (ports of
 * agent/credential_persistence.py: _normalize_key, _is_secret_payload_key).
 * Deterministic key normalization / classification (no FS / network).
 */

#include "credential_persistence.h"
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

        if (strcmp(op, "norm") == 0) {
            char out[256];
            credential_normalize_key(v, out, sizeof(out));
            printf("{\"op\":\"norm\",\"in\":");
            emit_json_string(v);
            printf(",\"out\":");
            emit_json_string(out);
            printf("}\n");
        } else if (strcmp(op, "secret") == 0) {
            int r = is_secret_payload_key(v);
            printf("{\"op\":\"secret\",\"in\":");
            emit_json_string(v);
            printf(",\"out\":%s}\n", r ? "true" : "false");
        } else if (strcmp(op, "fp") == 0) {
            char *fp = fingerprint_value(v);
            printf("{\"op\":\"fp\",\"in\":");
            emit_json_string(v);
            printf(",\"out\":");
            if (fp) emit_json_string(fp);
            else printf("null");
            printf("}\n");
            free(fp);
        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
