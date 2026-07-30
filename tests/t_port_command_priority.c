/*
 * t_port_command_priority.c — oracle harness for the PURE Telegram menu
 * prioritization helpers in src/cli/gateway_command_sanitize.c:
 *   commands_telegram_effective_priority(menu_cfg_json)  (port of
 *     hermes_cli/commands.py:_telegram_effective_priority)
 *   commands_prioritize_telegram_menu(entries, n, menu_cfg_json) (port of
 *     hermes_cli/commands.py:_prioritize_telegram_menu_commands)
 * Deterministic given the menu config JSON + entry list.
 */

#include "gateway_command_sanitize.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXN 128

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

/* read a whole stdin line (already has no trailing newline) into a malloc buf */
static char *dup_line(const char *s) { return strdup(s); }

int main(void) {
    char line[8192];
    /* buffer the cfg + entries for the current case */
    char *case_cfg = NULL;
    cmd_entry_t *case_entries = malloc(sizeof(cmd_entry_t) * MAXN);
    int case_n = 0;

    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line) continue;

        if (strncmp(line, "===", 3) == 0) {
            /* flush case */
            /* effective priority */
            char **pri = commands_telegram_effective_priority(case_cfg ? case_cfg : "", &(int){0});
            int npri = 0;
            while (pri && pri[npri]) npri++;
            printf("{\"op\":\"priority\",\"priority\":[");
            for (int i = 0; i < npri; i++) {
                if (i) printf(",");
                emit_json_string(pri[i]);
            }
            printf("]}");
            for (int i = 0; i < npri; i++) free(pri[i]);
            free(pri);
            printf("\n");

            /* sort */
            if (case_n > 0) {
                cmd_entry_t *work = malloc(sizeof(cmd_entry_t) * (size_t)case_n);
                for (int i = 0; i < case_n; i++) {
                    memset(&work[i], 0, sizeof(work[i]));
                    strncpy(work[i].name, case_entries[i].name, CMD_NAME_LIMIT);
                    work[i].name[CMD_NAME_LIMIT] = '\0';
                    work[i].description = case_entries[i].description ? strdup(case_entries[i].description) : NULL;
                    work[i].key = case_entries[i].key ? strdup(case_entries[i].key) : NULL;
                }
                commands_prioritize_telegram_menu(work, case_n, case_cfg ? case_cfg : "");
                printf("{\"op\":\"sort\",\"order\":[");
                for (int i = 0; i < case_n; i++) {
                    if (i) printf(",");
                    emit_json_string(work[i].name);
                }
                printf("]}");
                for (int i = 0; i < case_n; i++) { free(work[i].description); free(work[i].key); }
                free(work);
                printf("\n");
            }

            /* reset */
            for (int i = 0; i < case_n; i++) { free(case_entries[i].description); free(case_entries[i].key); }
            case_n = 0;
            free(case_cfg); case_cfg = NULL;
            continue;
        }

        if (strncmp(line, "cfg ", 4) == 0) {
            free(case_cfg);
            case_cfg = dup_line(line + 4);
            continue;
        }
        if (strncmp(line, "entry ", 6) == 0) {
            char *e = line + 6;
            char *bar = strchr(e, '|');
            char *desc = "", *key = "";
            if (bar) {
                *bar = '\0';
                char *rest = bar + 1;
                char *bar2 = strchr(rest, '|');
                if (bar2) { *bar2 = '\0'; desc = rest; key = bar2 + 1; }
                else desc = rest;
            }
            case_entries[case_n].name[0] = '\0';
            strncpy(case_entries[case_n].name, e, CMD_NAME_LIMIT);
            case_entries[case_n].name[CMD_NAME_LIMIT] = '\0';
            case_entries[case_n].description = *desc ? strdup(desc) : NULL;
            case_entries[case_n].key = *key ? strdup(key) : NULL;
            case_n++;
            continue;
        }
    }
    /* flush trailing case */
    if (case_cfg || case_n > 0) {
        char **pri = commands_telegram_effective_priority(case_cfg ? case_cfg : "", &(int){0});
        int npri = 0;
        while (pri && pri[npri]) npri++;
        printf("{\"op\":\"priority\",\"priority\":[");
        for (int i = 0; i < npri; i++) {
            if (i) printf(",");
            emit_json_string(pri[i]);
        }
        printf("]}");
        for (int i = 0; i < npri; i++) free(pri[i]);
        free(pri);
        printf("\n");

        if (case_n > 0) {
            cmd_entry_t *work = malloc(sizeof(cmd_entry_t) * (size_t)case_n);
            for (int i = 0; i < case_n; i++) {
                memset(&work[i], 0, sizeof(work[i]));
                strncpy(work[i].name, case_entries[i].name, CMD_NAME_LIMIT);
                work[i].name[CMD_NAME_LIMIT] = '\0';
                work[i].description = case_entries[i].description ? strdup(case_entries[i].description) : NULL;
                work[i].key = case_entries[i].key ? strdup(case_entries[i].key) : NULL;
            }
            commands_prioritize_telegram_menu(work, case_n, case_cfg ? case_cfg : "");
            printf("{\"op\":\"sort\",\"order\":[");
            for (int i = 0; i < case_n; i++) {
                if (i) printf(",");
                emit_json_string(work[i].name);
            }
            printf("]}");
            for (int i = 0; i < case_n; i++) { free(work[i].description); free(work[i].key); }
            free(work);
            printf("\n");
        }
        for (int i = 0; i < case_n; i++) { free(case_entries[i].description); free(case_entries[i].key); }
        free(case_cfg);
    }
    free(case_entries);
    return 0;
}
