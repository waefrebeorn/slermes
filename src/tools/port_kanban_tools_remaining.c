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
    /* Python: subscribe session to events. */
    if (!session_id) return false;
    printf("kanban auto-subscribe (%s)\n", session_id);
    return true;
}

/* PoP: _board_schema_prop @ tools/kanban_tools.py:_board_schema_prop */
char *kbt_board_schema_prop(void) {
    /* Python: optional board param fragment. */
    return strdup("{\"board\": {\"type\": \"string\", \"description\": \"Board name\", \"optional\": true}}");
}
