/*
 * plugin_model_providers.c — Model providers plugin.
 * Port of Python plugins/model-providers/ (provider SDK wrappers).
 *
 * Registers 28+ model providers with the provider system.
 * The C equivalent is in src/provider/ (google_oauth.c, copilot_oauth.c,
 * token_exchange.c) + src/agent/provider_*.c for each backend.
 *
 * Port of Python plugins/model-providers: C handles in src/provider/ + src/agent/provider_*.c
 */
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Plugin metadata ──────────────────────────────────────────── */

const char *plugin_meta_name(void) {
    return "model-providers";
}

const char *plugin_meta_version(void) {
    return "0.3.0";
}

const char *plugin_meta_type(void) {
    return "provider";
}

const char *plugin_meta_description(void) {
    return "28+ model provider backends — C impl in src/provider/ + src/agent/provider_*.c";
}

int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ── Init ─────────────────────────────────────────────────────── */

int plugin_init(void) {
    /* All provider backends are initialized from provider.c during agent startup. */
    return 0;
}
