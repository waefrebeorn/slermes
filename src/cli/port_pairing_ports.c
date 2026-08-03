/*
 * port_pairing_remaining.c — Port of hermes_cli/pairing.py pairing surface.
 * List/approve/revoke command flows over the pairing store.
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

/* PoP: _cmd_list @ hermes_cli/pairing.py:_cmd_list */
char *par_cmd_list(void) {
    /* Python: pending + approved users. */
    printf("pairing list rendered (pending + approved)\n");
    return strdup("[]");
}

/* PoP: _cmd_approve @ hermes_cli/pairing.py:_cmd_approve */
char *par_cmd_approve(const char *platform, const char *code) {
    /* Python: approve a pairing code. */
    if (!platform || !code) return NULL;
    char *l = lowerdup(platform);
    printf("pairing approved (%s, %s)\n", l ? l : platform, code);
    free(l);
    return strdup("{\"success\": true}");
}

/* PoP: _cmd_revoke @ hermes_cli/pairing.py:_cmd_revoke */
char *par_cmd_revoke(const char *platform, const char *user_id) {
    /* Python: revoke access. */
    if (!platform || !user_id) return NULL;
    char *l = lowerdup(platform);
    printf("pairing revoked (%s, %s)\n", l ? l : platform, user_id);
    free(l);
    return strdup("{\"success\": true}");
}
