/*
 * port_nous_account_remaining.c — Port of hermes_cli/nous_account.py
 * coercion surface. Strict bool/int/float coercion.
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

/* PoP: _coerce_bool @ hermes_cli/nous_account.py:_coerce_bool */
bool nac_coerce_bool(const char *value) {
    /* Python: bool only; else None. */
    if (!value) return false;
    return strcmp(value, "true") == 0;
}

/* PoP: _coerce_int @ hermes_cli/nous_account.py:_coerce_int */
long nac_coerce_int(const char *value) {
    /* Python: strict int, no bool. */
    if (!value) return -1;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return -1;
    return v;
}

/* PoP: _coerce_float @ hermes_cli/nous_account.py:_coerce_float */
double nac_coerce_float(const char *value) {
    /* Python: strict float, no bool. */
    if (!value) return -1.0;
    char *end = NULL;
    double v = strtod(value, &end);
    if (end == value || *end != '\0') return -1.0;
    return v;
}
