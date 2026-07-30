/*
 * t_port_tool_search.c — oracle harness for the PURE tokenization / token
 * estimation helpers in src/tools/tool_search.c (ports of tools/tool_search.py:
 * _tokenize, estimate_tokens_from_schemas). Deterministic.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_tool_search.h"

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

/* Decode \n token to a real newline (fixture uses literal \n). */
static char *decode_nl(const char *in) {
    if (!in) return NULL;
    size_t n = strlen(in);
    char *out = malloc(n + 1);
    size_t oi = 0;
    for (size_t i = 0; i < n; ) {
        if (strncmp(in + i, "\\n", 2) == 0) { out[oi++] = '\n'; i += 2; }
        else out[oi++] = in[i++];
    }
    out[oi] = '\0';
    return out;
}

int main(void) {
    char line[16384];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        char op[40];
        const char *rest;
        /* split op on first space */
        size_t i = 0;
        while (line[i] && line[i] != ' ') i++;
        memcpy(op, line, i < sizeof(op) ? i : sizeof(op) - 1);
        op[i < sizeof(op) ? i : sizeof(op) - 1] = '\0';
        rest = (line[i] == ' ') ? line + i + 1 : "";

        if (strcmp(op, "tok") == 0) {
            char *d = decode_nl(rest);
            char toks[64][64];
            int n = tool_search_tokenize(d ? d : "", toks, 64);
            printf("{\"op\":\"tok\",\"in\":");
            emit_json_string(d ? d : "");
            printf(",\"tokens\":[");
            for (int k = 0; k < n; k++) {
                if (k) putchar(',');
                emit_json_string(toks[k]);
            }
            printf("]}\n");
            free(d);
        } else if (strcmp(op, "est") == 0) {
            char *d = decode_nl(rest);
            int t = tool_search_estimate_tokens(d ? d : "");
            printf("{\"op\":\"est\",\"in\":");
            emit_json_string(d ? d : "");
            printf(",\"tokens\":%d}\n", t);
            free(d);
        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
