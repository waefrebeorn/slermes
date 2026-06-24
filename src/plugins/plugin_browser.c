/*
 * plugin_browser.c — Browser automation plugin.
 * Port of Python plugins/browser/ (3 providers: browser_use, firecrawl, browserbase).
 *
 * The C equivalent is in src/agent/browser_registry.c + src/tools/browser.c.
 * browser_registry.c handles provider discovery; tools/browser.c handles browser automation.
 * Camofox, CDP, dialog, and supervisor features are available via tools/browser.c.
 *
 * PoP: Python plugins/browser/ → C tools/browser.c + agent/browser_registry.c
 */
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *plugin_meta_name(void) {
    return "browser";
}
const char *plugin_meta_version(void) {
    return "0.3.0";
}
const char *plugin_meta_type(void) {
    return "tool";
}
const char *plugin_meta_description(void) {
    return "Browser automation (browser_use, firecrawl, browserbase) — C impl in src/tools/browser.c";
}
int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }
int plugin_init(void) {
    /* browser_registry_init() called from agent startup. */
    return 0;
}
