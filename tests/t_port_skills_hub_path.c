/*
 * t_port_skills_hub_path.c — oracle harness for the PURE path-validation
 * helpers in src/skills_hub.c (faithful port of tools/skills_hub.py
 * _normalize_bundle_path / _validate_skill_name / _validate_install_parent_path
 * / _normalize_lock_install_path).
 *
 * These are deterministic string/path transforms (no FS, no network). They
 * exercise the REAL C functions and emit stable JSON (one object per line)
 * that the Python oracle reproduces.
 */

#include "hermes_skills_hub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef HERMES_PATH_MAX
#define HERMES_PATH_MAX 4096
#endif

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
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        char op[40];
        const char *rest;
        split_kv(line, op, sizeof(op), &rest);
        const char *v = rest[0] ? rest : "";

        if (strcmp(op, "validate_skill") == 0) {
            char out[HERMES_PATH_MAX];
            bool ok = hub_normalize_skill_name(v, out, sizeof(out));
            printf("{\"op\":\"validate_skill\",\"in\":");
            emit_json_string(v); printf(",\"ok\":%s,\"out\":", ok ? "true" : "false");
            emit_json_string(ok ? out : ""); printf("}\n");

        } else if (strcmp(op, "validate_parent") == 0) {
            char out[HERMES_PATH_MAX];
            bool ok = hub_validate_install_parent_path(v, out, sizeof(out));
            printf("{\"op\":\"validate_parent\",\"in\":");
            emit_json_string(v); printf(",\"ok\":%s,\"out\":", ok ? "true" : "false");
            emit_json_string(ok ? out : ""); printf("}\n");

        } else if (strcmp(op, "lock_path") == 0) {
            /* lock_path <skill>|<install_path> */
            const char *bar = strchr(v, '|');
            char *skill = strdup(v);
            char *ipath = strdup("");
            if (bar) { skill[bar - v] = '\0'; ipath = strdup(bar + 1); }
            char out[HERMES_PATH_MAX];
            bool ok = hub_normalize_lock_install_path(ipath, skill, out, sizeof(out));
            printf("{\"op\":\"lock_path\",\"skill\":");
            emit_json_string(skill); printf(",\"install_path\":");
            emit_json_string(ipath); printf(",\"ok\":%s,\"out\":", ok ? "true" : "false");
            emit_json_string(ok ? out : ""); printf("}\n");
            free(skill); free(ipath);

        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
