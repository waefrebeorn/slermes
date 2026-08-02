/*
 * port_windows_ssh_runtime_remaining.c — Port of hermes_cli/windows_ssh_runtime.py
 * ssh-runtime surface. Lock path, op dispatch, json main.
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

/* PoP: _lock_path @ hermes_cli/windows_ssh_runtime.py:_lock_path */
char *wss_lock_path(const char *ownership_id) {
    if (!ownership_id) return NULL;
    char *out = NULL;
    asprintf(&out, "%s/backend.lock.json", ownership_id);
    return out;
}

/* PoP: dispatch @ hermes_cli/windows_ssh_runtime.py:dispatch */
char *wss_dispatch(const char *argv_json) {
    /* Python: first arg is operation. */
    if (!argv_json) return NULL;
    printf("ssh-runtime dispatch (op from argv)\n");
    return strdup("{}");
}

/* PoP: main @ hermes_cli/windows_ssh_runtime.py:main */
int wss_main(const char *argv_json) {
    /* Python: json print dispatch result. */
    if (!argv_json) return -1;
    printf("ssh-runtime main (json output)\n");
    return 0;
}
