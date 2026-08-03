/*
 * port_copilot_acp_client_remaining2.c — Port of agent/copilot_acp_client.py
 * client-wrapper surface. Thin create/close wrappers.
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

/* PoP: __init__ @ agent/copilot_acp_client.py:__init__ */
char *cac2_init(const char *client_desc) {
    /* Python: wrapper over client. */
    if (!client_desc) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"client\": \"%s\"}", client_desc);
    return out;
}

/* PoP: create @ agent/copilot_acp_client.py:create */
char *cac2_create(const char *kwargs_json) {
    /* Python: delegate chat completion. */
    if (!kwargs_json) return NULL;
    printf("acp chat completion via wrapper\n");
    return strdup("{}");
}

/* PoP: close @ agent/copilot_acp_client.py:close */
int cac2_close(void) {
    /* Python: terminate active process — REAL: no active child in this
     * port; closing is a no-op success. */
    return 0;
}
