/*
 * t_port_command_clamp.c — oracle harness for the PURE command-name clamp
 * helper commands_clamp_names in src/cli/gateway_command_sanitize.c (faithful
 * port of hermes_cli/commands.py:_clamp_command_names). Deterministic
 * length-clamp + collision avoidance over name/desc/key entries.
 */

#include "gateway_command_sanitize.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXN 64

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

int main(void) {
    /* Free-form: read a block of lines; entries are "name|desc|key".
     * A line "RESERVED:" begins a reserved-name list. A blank line between
     * blocks starts a new case. */
    char line[2048];
    cmd_entry_t *entries = malloc(sizeof(cmd_entry_t) * MAXN);
    int n = 0;
    char **reserved = malloc(sizeof(char *) * MAXN);
    int nr = 0;
    int case_no = 0;

    /* flush a case */
    char *block_entries[MAXN];
    int block_n = 0;

    /* We'll accumulate into a single case per blank-line-separated group. */
    /* Simpler: each non-blank line is an entry; "reserved:" sets reserved.
       Cases are separated by a line "===". */
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';

        if (strncmp(line, "===", 3) == 0) {
            /* run the case */
            cmd_entry_t *out = malloc(sizeof(cmd_entry_t) * (size_t)(block_n + nr + 1));
            int dropped = 0;
            int written = commands_clamp_names((const cmd_entry_t *)entries, block_n,
                                               (const char *const *)reserved, nr,
                                               out, block_n + nr + 1, &dropped);
            printf("{\"case\":%d,\"dropped\":%d,\"kept\":[", case_no, dropped);
            for (int i = 0; i < written; i++) {
                if (i) printf(",");
                printf("{");
                printf("\"name\":"); emit_json_string(out[i].name);
                printf(",\"desc\":"); emit_json_string(out[i].description);
                printf(",\"key\":"); emit_json_string(out[i].key);
                printf("}");
                free(out[i].description); free(out[i].key);
            }
            printf("]}\n");
            free(out);
            /* reset */
            for (int i = 0; i < block_n; i++) { free(entries[i].description); free(entries[i].key); }
            block_n = 0; nr = 0; case_no++;
            continue;
        }
        if (!*line) continue;

        if (strncmp(line, "reserved:", 9) == 0) {
            char *r = line + 9;
            while (*r == ' ') r++;
            reserved[nr++] = strdup(r);
            continue;
        }
        /* entry: name|desc|key (desc/key optional) */
        char *name = line;
        char *bar = strchr(line, '|');
        char *desc = "", *key = "";
        if (bar) {
            *bar = '\0';
            char *rest = bar + 1;
            char *bar2 = strchr(rest, '|');
            if (bar2) { *bar2 = '\0'; desc = rest; key = bar2 + 1; }
            else desc = rest;
        }
        entries[block_n].name[0] = '\0';
        strncpy(entries[block_n].name, name, CMD_NAME_LIMIT);
        entries[block_n].name[CMD_NAME_LIMIT] = '\0';
        entries[block_n].description = *desc ? strdup(desc) : NULL;
        entries[block_n].key = *key ? strdup(key) : NULL;
        block_n++;
    }
    /* flush trailing case if any */
    if (block_n > 0 || nr > 0) {
        cmd_entry_t *out = malloc(sizeof(cmd_entry_t) * (size_t)(block_n + nr + 1));
        int dropped = 0;
        int written = commands_clamp_names((const cmd_entry_t *)entries, block_n,
                                           (const char *const *)reserved, nr,
                                           out, block_n + nr + 1, &dropped);
        printf("{\"case\":%d,\"dropped\":%d,\"kept\":[", case_no, dropped);
        for (int i = 0; i < written; i++) {
            if (i) printf(",");
            printf("{");
            printf("\"name\":"); emit_json_string(out[i].name);
            printf(",\"desc\":"); emit_json_string(out[i].description);
            printf(",\"key\":"); emit_json_string(out[i].key);
            printf("}");
            free(out[i].description); free(out[i].key);
        }
        printf("]}\n");
        free(out);
        for (int i = 0; i < block_n; i++) { free(entries[i].description); free(entries[i].key); }
    }
    free(entries); free(reserved);
    return 0;
}
