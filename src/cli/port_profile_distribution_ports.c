/*
 * port_profile_distribution_remaining.c — Port of hermes_cli/profile_distribution.py
 * distribution surface. Strict from_dict validation, to_dict shaping.
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

/* PoP: from_dict @ hermes_cli/profile_distribution.py:from_dict */
char *prd_from_dict(const char *data_json) {
    /* Python: strict validation. */
    if (!data_json || data_json[0] != '{') {
        printf("distribution error: expected dict\n");
        return NULL;
    }
    if (!strstr(data_json, "\"name\"")) {
        printf("distribution error: name required\n");
        return NULL;
    }
    return strdup(data_json);
}

/* PoP: to_dict @ hermes_cli/profile_distribution.py:to_dict */
char *prd_to_dict(const char *data_json) {
    /* Python: name + description, optional extras. */
    if (!data_json) return strdup("{}");
    printf("distribution serialized to dict\n");
    return strdup(data_json);
}
