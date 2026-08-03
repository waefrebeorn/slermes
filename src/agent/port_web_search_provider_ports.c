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
#include "web_search_registry.h"

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
    /* Python: any registered web search provider can service calls. */
    return web_search_provider_count() > 0;
}

/* PoP: search @ agent/web_search_provider.py:search */
char *wsp_search(const char *query, long limit) {
    /* Python: execute web search via the active provider. */
    if (!query) return NULL;
    const web_search_provider_t *p = web_search_get_active("search");
    if (!p || !(p->capabilities & WEB_CAP_SEARCH)) return strdup("{\"results\": []}");
    /* The registry doesn't carry a search callback; delegate to the
     * real web_search tool which uses the configured backend. */
    extern char *web_search_tool(const char *query, long limit);
    if (web_search_tool) return web_search_tool(query, limit);
    return strdup("{\"results\": []}");
}

/* PoP: extract @ agent/web_search_provider.py:extract */
char *wsp_extract(const char *urls_json) {
    /* Python: extract content from URLs via the active provider. */
    if (!urls_json) return NULL;
    const web_search_provider_t *p = web_search_get_active("extract");
    if (!p || !(p->capabilities & WEB_CAP_EXTRACT)) return strdup("{}");
    extern char *web_extract_tool(const char *urls_json);
    if (web_extract_tool) return web_extract_tool(urls_json);
    return strdup("{}");
}
