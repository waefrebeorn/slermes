/*
 * t_port_url_safety.c — oracle harness for the PURE, deterministic helpers in
 * src/cli/port_tools_url_safety.c (port of tools/url_safety.py).
 *
 * Oracle-viable subset (no DNS / network during the checked paths):
 *   normalize   -> cli_tools_url_safety_normalize_url_for_request (pure)
 *   blocked_ip  -> cli_tools_url_safety__is_blocked_ip (pure)
 *   always_blocked -> cli_tools_url_safety_is_always_blocked_url
 *                    (literal-IP + blocked-hostname paths only; hostname
 *                     paths do getaddrinfo and are intentionally NOT oracled)
 *
 * One op per line; the harness exercises the REAL C functions and emits
 * stable JSON (one object per line) that the Python oracle reproduces.
 */

#include "url_safety_cli.h"   /* cli_tools_url_safety_* decls */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* emit a JSON string value (raw quotes) */
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

/* split a line into op + rest (first space separates) */
static void split_op(const char *line, char *op, size_t opsz, const char **rest) {
    size_t i = 0;
    while (*line && *line != ' ' && i + 1 < opsz) op[i++] = *line++;
    op[i] = '\0';
    if (*line == ' ') line++;
    *rest = line;
}

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        char op[32];
        const char *rest;
        split_op(line, op, sizeof(op), &rest);

        if (strcmp(op, "normalize") == 0) {
            char buf[8192];
            int rc = cli_tools_url_safety_normalize_url_for_request(
                rest[0] ? rest : "", buf, sizeof(buf));
            printf("{\"op\":\"normalize\",\"in\":");
            emit_json_string(rest[0] ? rest : "");
            printf(",\"out\":");
            if (rc == 0) emit_json_string(buf);
            else emit_json_string(rest[0] ? rest : "");  /* C returns -1; Python returns raw */
            printf(",\"rc\":%d}\n", rc);

        } else if (strcmp(op, "blocked_ip") == 0) {
            int r = cli_tools_url_safety__is_blocked_ip(rest[0] ? rest : "");
            printf("{\"op\":\"blocked_ip\",\"ip\":");
            emit_json_string(rest[0] ? rest : "");
            printf(",\"blocked\":%s}\n", r ? "true" : "false");

        } else if (strcmp(op, "always_blocked") == 0) {
            int r = cli_tools_url_safety_is_always_blocked_url(rest[0] ? rest : "");
            printf("{\"op\":\"always_blocked\",\"url\":");
            emit_json_string(rest[0] ? rest : "");
            printf(",\"blocked\":%s}\n", r ? "true" : "false");

        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
