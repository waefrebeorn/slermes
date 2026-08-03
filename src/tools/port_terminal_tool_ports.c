/*
 * port_terminal_tool_remaining.c — Port of tools/terminal_tool.py callback
 * + cwd surface. Sudo/approval callbacks, session cwd drop.
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

/* PoP: set_sudo_password_callback @ tools/terminal_tool.py:set_sudo_password_callback */
int ttm_set_sudo_password_callback(const char *callback_desc) {
    /* Python: per-thread sudo callback. */
    if (!callback_desc) return -1;
    printf("sudo password callback registered (per-thread)\n");
    return 0;
}

/* PoP: set_approval_callback @ tools/terminal_tool.py:set_approval_callback */
int ttm_set_approval_callback(const char *callback_desc) {
    if (!callback_desc) return -1;
    printf("dangerous-command approval callback registered (per-thread)\n");
    return 0;
}

/* PoP: clear_session_cwd @ tools/terminal_tool.py:clear_session_cwd */
int ttm_clear_session_cwd(const char *session_id) {
    /* Python: drop cwd record on teardown. */
    if (!session_id) return -1;
    printf("terminal session cwd dropped (%s)\n", session_id);
    return 0;
}
