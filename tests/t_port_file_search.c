/*
 * t_port_file_search.c — oracle harness for the PURE file-search diagnostics
 * helpers in src/tools/port_file_operations_search.c (port of
 * tools/file_operations.py: _search_stdout_and_limit, _split_tool_diagnostics).
 * Deterministic string/regex transforms over an ExecuteResult-shaped struct.
 */

#include "tools/port_file_operations_search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Decode \n tokens (and a couple of others) inside fixture content into real
 * newlines, matching the Python oracle's handling. */
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

        if (strcmp(op, "search") == 0) {
            /* search <exit>|<stdout> */
            const char *bar = strchr(rest, '|');
            int exit_code = 0;
            char *stdout_s = strdup("");
            if (bar) {
                exit_code = atoi(rest);
                stdout_s = decode_nl(bar + 1);
            }
            file_ops_execute_result_t res;
            res.exit_code = exit_code;
            res.stdout = stdout_s;
            res.stderr = NULL;
            char *reason = NULL;
            char *out = file_ops_search_search_stdout_and_limit(&res, &reason);
            printf("{\"op\":\"search\",\"exit_code\":%d,\"reason\":", exit_code);
            emit_json_string(reason ? reason : "");
            printf(",\"out\":");
            emit_json_string(out ? out : "");
            printf("}\n");
            free(out); free(reason); free(stdout_s);

        } else if (strcmp(op, "split") == 0) {
            char *content = decode_nl(rest[0] ? rest : "");
            char *diag = NULL, *pay = NULL;
            file_ops_search_split_tool_diagnostics(content, &diag, &pay);
            printf("{\"op\":\"split\",\"diagnostics\":");
            emit_json_string(diag ? diag : "");
            printf(",\"payload\":");
            emit_json_string(pay ? pay : "");
            printf("}\n");
            free(diag); free(pay); free(content);

        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
