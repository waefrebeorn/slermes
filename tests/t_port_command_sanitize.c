/*
 * t_port_command_sanitize.c — oracle harness for the PURE command-name
 * sanitizers in src/cli/gateway_command_sanitize.c (port of
 * hermes_cli/commands.py: _sanitize_telegram_name / _sanitize_slack_name).
 * Deterministic string transforms (no IO / network).
 */

#include "gateway_command_sanitize.h"
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

        char *out = NULL;
        if (strcmp(op, "telegram") == 0) {
            out = commands_sanitize_telegram_name(v);
        } else if (strcmp(op, "slack") == 0) {
            out = commands_sanitize_slack_name(v);
        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
            continue;
        }
        printf("{\"op\":\"%s\",\"in\":", op);
        emit_json_string(v); printf(",\"out\":");
        emit_json_string(out ? out : "");
        printf("}\n");
        free(out);
    }
    return 0;
}
