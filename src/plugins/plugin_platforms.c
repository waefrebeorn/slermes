/*
 * plugin_platforms.c — Platform adapters plugin.
 * Port of Python plugins/platforms/ (platform SDK wrappers).
 *
 * Provides gateway platform adapters (Discord, Mattermost, Teams, NTFY,
 * Simplex, IRC, Google Chat, LINE, Home Assistant).
 * The C equivalent is in src/gateway/platforms/ (22 adapters).
 *
 * Port of Python plugins/platforms: C handles in src/gateway/platforms/
 */
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Plugin metadata ──────────────────────────────────────────── */

const char *plugin_meta_name(void) {
    return "platforms";
}

const char *plugin_meta_version(void) {
    return "0.3.0";
}

const char *plugin_meta_type(void) {
    return "platform";
}

const char *plugin_meta_description(void) {
    return "10+ gateway platform adapters — C impl in src/gateway/platforms/ (22 adapters)";
}

int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ── Init ─────────────────────────────────────────────────────── */

int plugin_init(void) {
    /* Gateway platform adapters are registered during gateway_lifecycle_init(). */
    return 0;
}
