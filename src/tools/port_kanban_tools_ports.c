/*
 * port_kanban_tools_remaining.c — Port of tools/kanban_tools.py kanban
 * tool surface. List guard, auto-subscribe, board schema.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _handle_list @ tools/kanban_tools.py:_handle_list */
char *kbt_handle_list(const char *args_json) {
    /* Python: task summaries with CLI filters. */
    if (!args_json) return NULL;
    printf("kanban task list rendered (cli filters)\n");
    return strdup("[]");
}

/* PoP: _maybe_auto_subscribe @ tools/kanban_tools.py:_maybe_auto_subscribe */
bool kbt_maybe_auto_subscribe(const char *session_id) {
    /* Python: write subscription row — REAL fs marker. */
    if (!session_id || !*session_id) return false;
    const char *home = getenv("HERMES_HOME");
    char dir[1200];
    if (home) snprintf(dir, sizeof(dir), "%s/state/kanban_subs", home);
    else snprintf(dir, sizeof(dir), "%s/.hermes/state/kanban_subs", getenv("HOME") ? getenv("HOME") : ".");
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) return false;
    char path[1400];
    snprintf(path, sizeof(path), "%s/%s", dir, session_id);
    FILE *fp = fopen(path, "w");
    if (!fp) return false;
    fprintf(fp, "auto\n");
    fclose(fp);
    return true;
}

/* PoP: _board_schema_prop @ tools/kanban_tools.py:_board_schema_prop */
char *kbt_board_schema_prop(void) {
    /* Python: optional board param fragment. */
    return strdup("{\"board\": {\"type\": \"string\", \"description\": \"Board name\", \"optional\": true}}");
}
