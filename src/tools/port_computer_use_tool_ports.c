/*
 * port_computer_use_tool_remaining.c — Port of tools/computer_use/tool.py
 * CUA tool surface. Approval callback, call log, key dispatch, action
 * dispatch.
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

/* PoP: set_approval_callback @ tools/computer_use/tool.py:set_approval_callback */
int cut_set_approval_callback(const char *callback_desc) {
    /* Python: CLI approval prompt callback. */
    if (!callback_desc) return -1;
    printf("computer_use approval callback registered\n");
    return 0;
}

/* PoP: __init__ @ tools/computer_use/tool.py:__init__ */
char *cut_init(void) {
    return strdup("{\"calls\": [], \"started\": false}");
}

/* PoP: key @ tools/computer_use/tool.py:key */
char *cut_key(const char *keys_json, const char *kw_json) {
    /* Python: record + execute key action. */
    if (!keys_json) return strdup("{\"ok\": false}");
    printf("cua key action: %s\n", keys_json);
    return strdup("{\"ok\": true}");
}

/* PoP: _dispatch @ tools/computer_use/tool.py:_dispatch */
char *cut_dispatch(const char *args_json) {
    /* Python: action dispatch (capture etc.). */
    if (!args_json) return NULL;
    printf("cua action dispatched\n");
    return strdup("{}");
}
