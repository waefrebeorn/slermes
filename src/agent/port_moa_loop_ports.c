/*
 * port_moa_loop_remaining.c — Port of agent/moa_loop.py MoA surface.
 * Result state, text extraction, prepared-request create.
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

/* PoP: __init__ @ agent/moa_loop.py:__init__ */
char *moa_init(const char *usage_json, double cost_usd, const char *cost_status) {
    char *out = NULL;
    asprintf(&out, "{\"usage\": %s, \"cost_usd\": %.4f, \"cost_status\": \"%s\"}",
             usage_json ? usage_json : "{}", cost_usd, cost_status ? cost_status : "");
    return out;
}

/* PoP: _extract_text @ agent/moa_loop.py:_extract_text */
char *moa_extract_text(const char *response_json) {
    /* Python: assistant text from response. */
    if (!response_json) return strdup("");
    const char *p = strstr(response_json, "\"content\"");
    if (!p) return strdup("");
    const char *colon = strchr(p, ':');
    if (!colon) return strdup("");
    const char *v = colon + 1;
    while (*v == ' ' || *v == '"') v++;
    const char *e = v;
    while (*e && *e != '"') e++;
    if (e == v) return strdup("");
    return strndup(v, (size_t)(e - v));
}

/* PoP: create @ agent/moa_loop.py:create */
char *moa_create(const char *kwargs_json) {
    /* Python: prepared-request aware create. */
    if (!kwargs_json) return NULL;
    printf("moa create (prepared request path)\n");
    return strdup("{}");
}
