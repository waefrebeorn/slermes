/*
 * t_port_github_provider.c — oracle harness for the PURE github_provider_for
 * helper in src/skills_hub.c (port of tools/skills_hub.py:github_provider_for).
 * Deterministic string->label lookup (owner/repo, case-insensitive).
 */

#include "hermes_skills_hub.h"
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

int main(void) {
    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;
        const char *out = github_provider_for(line[0] ? line : "");
        printf("{\"in\":");
        emit_json_string(line[0] ? line : "");
        printf(",\"out\":");
        emit_json_string(out ? out : NULL);
        printf("}\n");
    }
    return 0;
}
