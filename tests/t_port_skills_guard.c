/*
 * t_port_skills_guard.c — oracle harness for the PURE skill-trust helpers in
 * src/cli/port_tools_skills_guard.c (ports of tools/skills_guard.py:
 * _resolve_trust_level, _determine_verdict). Deterministic trust/verdict
 * classification (no FS / network).
 */

#include "skills_guard.h"
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

        if (strcmp(op, "trust") == 0) {
            const char *v = rest[0] ? rest : "";
            const char *r = cli_tools_skills_guard__resolve_trust_level(v, NULL);
            printf("{\"op\":\"trust\",\"in\":");
            emit_json_string(v);
            printf(",\"out\":");
            emit_json_string(r ? r : "");
            printf("}\n");
        } else if (strcmp(op, "verdict") == 0) {
            int sev = 0, fnd = 0;
            sscanf(rest, "%d %d", &sev, &fnd);
            const char *r = cli_tools_skills_guard__determine_verdict(sev, fnd);
            printf("{\"op\":\"verdict\",\"severity\":%d,\"findings\":%d,\"out\":", sev, fnd);
            emit_json_string(r ? r : "");
            printf("}\n");
        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
