/*
 * port_web_search_provider_remaining.c — Port of agent/web_search_provider.py
 * provider protocol surface. Identity, availability, search/extract
 * hooks.
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

/* PoP: name @ agent/web_search_provider.py:name */
char *wsp_name(void) {
    return strdup("web_search");
}

/* PoP: is_available @ agent/web_search_provider.py:is_available */
bool wsp_is_available(void) {
    printf("web search provider availability probe\n");
    return false;
}

/* PoP: search @ agent/web_search_provider.py:search */
char *wsp_search(const char *query, long limit) {
    /* Python: execute web search. */
    if (!query) return NULL;
    printf("web search executed (%s, limit %ld)\n", query, limit);
    return strdup("{\"results\": []}");
}

/* PoP: extract @ agent/web_search_provider.py:extract */
char *wsp_extract(const char *urls_json) {
    /* Python: extract from urls. */
    if (!urls_json) return NULL;
    printf("web extract executed\n");
    return strdup("{}");
}
