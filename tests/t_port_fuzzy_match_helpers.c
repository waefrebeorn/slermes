/*
 * t_port_fuzzy_match_helpers.c — oracle harness for the PURE fuzzy-match
 * string helpers in src/cli/port_fuzzy_match_helpers.c (ports of
 * tools/fuzzy_match.py: _unicode_normalize, _leading_whitespace,
 * _first_meaningful_line). Deterministic string transforms (no IO / network).
 */

#include "fuzzy_match_helpers.h"
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
            default:
                if (c < 0x20) printf("\\u%04x", c);
                else putchar((int)c);
        }
    }
    putchar('"');
}

/* Decode \n / \t tokens (mirror of Python oracle). */
static char *decode_nl(const char *in) {
    if (!in) return NULL;
    size_t n = strlen(in);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t oi = 0;
    for (size_t i = 0; i < n; ) {
        if (strncmp(in + i, "\\n", 2) == 0) { out[oi++] = '\n'; i += 2; }
        else if (strncmp(in + i, "\\t", 2) == 0) { out[oi++] = '\t'; i += 2; }
        else out[oi++] = in[i++];
    }
    out[oi] = '\0';
    return out;
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

        if (strcmp(op, "norm") == 0) {
            char *out = fuzzy_match_unicode_normalize(v);
            printf("{\"op\":\"norm\",\"in\":");
            emit_json_string(v); printf(",\"out\":");
            emit_json_string(out ? out : "");
            printf("}\n");
            free(out);
        } else if (strcmp(op, "lead") == 0) {
            char *d = decode_nl(v);
            size_t n = fuzzy_match_leading_whitespace(d ? d : "");
            printf("{\"op\":\"lead\",\"in\":");
            emit_json_string(d ? d : ""); printf(",\"ws\":%zu}\n", n);
            free(d);
        } else if (strcmp(op, "first") == 0) {
            char *d = decode_nl(v);
            char *out = fuzzy_match_first_meaningful_line(d ? d : "");
            printf("{\"op\":\"first\",\"in\":");
            emit_json_string(d ? d : ""); printf(",\"out\":");
            emit_json_string(out ? out : "");
            printf("}\n");
            free(out); free(d);
        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
