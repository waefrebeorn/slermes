/*
 * t_port_path_security.c — oracle harness for the path-safety helpers in
 * src/cli/port_tools_path_security.c (ports of tools/path_security.py):
 *   cli_tools_path_security_has_traversal_component  (pure, no FS)
 *   cli_tools_path_security_validate_within_dir     (realpath-based; the
 *     harness materializes the temp tree it is asked to validate so the
 *     C realpath resolution matches the Python oracle's real FS).
 *
 * Fixture ops:
 *   traversal <path>            -> {"op":"traversal","in":...,"has":bool}
 *   within <root> | <relpath>   -> harness mkdir -p root, touches
 *                                   root/relpath (creating parent dirs), then
 *                                   validates root/relpath within root; emits
 *                                   {"op":"within","in":...,"error":<str|null>}
 */

#include "path_security.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

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

/* mkdir -p for a path (best-effort). */
static void mkdir_p(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t n = strlen(tmp);
    for (size_t i = 1; i < n; i++) {
        if (tmp[i] == '/') {
            char saved = tmp[i];
            tmp[i] = '\0';
            mkdir(tmp, 0755);
            tmp[i] = saved;
        }
    }
    mkdir(tmp, 0755);
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

        if (strcmp(op, "traversal") == 0) {
            const char *v = rest[0] ? rest : "";
            int r = cli_tools_path_security_has_traversal_component(v);
            printf("{\"op\":\"traversal\",\"in\":");
            emit_json_string(v);
            printf(",\"has\":%s}\n", r ? "true" : "false");
        } else if (strcmp(op, "within") == 0) {
            /* split on ' | ' */
            char buf[8192];
            snprintf(buf, sizeof(buf), "%s", rest);
            char *bar = strstr(buf, " | ");
            if (!bar) { printf("{\"op\":\"within\",\"error\":\"bad-fixture\"}\n"); continue; }
            *bar = '\0';
            const char *root = buf;
            const char *rel = bar + 3;

            /* materialize: root dir, then parent of relpath, then touch file */
            mkdir_p(root);
            char full[8192];
            snprintf(full, sizeof(full), "%s/%s", root, rel);
            char *ls = strrchr(full, '/');
            if (ls && ls != full) { *ls = '\0'; mkdir_p(full); *ls = '/'; }
            FILE *f = fopen(full, "w");
            if (f) { fputs("x", f); fclose(f); }

            char *err = cli_tools_path_security_validate_within_dir(full, root);
            printf("{\"op\":\"within\",\"in\":");
            emit_json_string(full);
            printf(",\"safe\":%s}\n", err ? "false" : "true");
            free(err);
        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
