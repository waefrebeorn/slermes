/*
 * port_kanban_diagnostics_remaining.c — Port of hermes_cli/kanban_diagnostics.py
 * diagnostic surface. to_dict shaping, positive-int coercion.
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

/* PoP: to_dict @ hermes_cli/kanban_diagnostics.py:to_dict */
char *kdi_to_dict(const char *kind, const char *label, const char *detail_json) {
    char *out = NULL;
    asprintf(&out, "{\"kind\": \"%s\", \"label\": \"%s\", \"detail\": %s}",
             kind ? kind : "", label ? label : "", detail_json ? detail_json : "{}");
    return out;
}

/* PoP: _positive_int @ hermes_cli/kanban_diagnostics.py:_positive_int */
long kdi_positive_int(const char *value, long fallback) {
    /* Python: parse or default. */
    if (!value) return fallback;
    long v = atol(value);
    return v > 0 ? v : fallback;
}
