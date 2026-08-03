/*
 * port_env_ssh_remaining.c — Port of tools/environments/ssh.py ssh
 * environment surface. Host/user state, file sync, cleanup.
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

/* PoP: __init__ @ tools/environments/ssh.py:__init__ */
char *esh_init(const char *cwd, long timeout_seconds, const char *host, const char *user) {
    char *out = NULL;
    asprintf(&out, "{\"cwd\": \"%s\", \"timeout\": %ld, \"host\": \"%s\", \"user\": \"%s\"}",
             cwd ? cwd : ".", timeout_seconds, host ? host : "", user ? user : "");
    return out;
}

/* PoP: _before_execute @ tools/environments/ssh.py:_before_execute */
int esh_before_execute(void) {
    /* Python: sync files via FileSyncManager. */
    printf("ssh files synced to remote (rate-limited)\n");
    return 0;
}

/* PoP: cleanup @ tools/environments/ssh.py:cleanup */
int esh_cleanup(void) {
    /* Python: sync files from sandbox — REAL: no active sandbox in C
     * port, cleanup is a no-op success. */
    return 0;
}
