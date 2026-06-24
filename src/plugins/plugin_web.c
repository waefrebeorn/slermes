/*
 * plugin_web.c — Web integration plugin.
 * Port of Python plugins/web/ (web search SDK wrappers).
 *
 * Provides web search through 8 providers (firecrawl, brave_free, parallel,
 * xai, exa, searxng, ddgs, tavily).
 * The C equivalent is in src/tools/web.c + lib/libhttp + web_search_registry.c.
 *
 * Port of Python plugins/web: C handles in tools/web.c + lib/libhttp + web_search_registry.c
 */
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Plugin metadata ──────────────────────────────────────────── */

const char *plugin_meta_name(void) {
    return "web";
}

const char *plugin_meta_version(void) {
    return "0.3.0";
}

const char *plugin_meta_type(void) {
    return "tool";
}

const char *plugin_meta_description(void) {
    return "Web search (8 providers) — C impl in tools/web.c + lib/libhttp + web_search_registry.c";
}

int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ── Init ─────────────────────────────────────────────────────── */

int plugin_init(void) {
    /* web_search_registry_init() called from tool_init.c during startup. */
    return 0;
}
