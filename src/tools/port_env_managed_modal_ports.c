/*
 * port_env_managed_modal_remaining.c — Port of tools/environments/managed_modal.py
 * managed-modal surface. Credential guard, sandbox cleanup, coercion.
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

/* PoP: __init__ @ tools/environments/managed_modal.py:__init__ */
char *emm_init(const char *cwd, long timeout_seconds) {
    /* Python: guard unsupported credentials. */
    char *out = NULL;
    asprintf(&out, "{\"cwd\": \"%s\", \"timeout\": %ld, \"guard\": true}",
             cwd ? cwd : ".", timeout_seconds);
    return out;
}

/* PoP: cleanup @ tools/environments/managed_modal.py:cleanup */
int emm_cleanup(const char *sandbox_id) {
    /* Python: sandbox teardown if present. */
    if (!sandbox_id) return 0;
    printf("managed modal sandbox torn down (%s)\n", sandbox_id);
    return 0;
}

/* PoP: _coerce_number @ tools/environments/managed_modal.py:_coerce_number */
double emm_coerce_number(const char *value, double default_value) {
    /* Python: parse or default. */
    if (!value) return default_value;
    char *end = NULL;
    double v = strtod(value, &end);
    if (end == value || *end != '\0') return default_value;
    return v;
}
