/*
 * plugin_context_engine.c — Context engine plugin.
 * Port of Python plugins/context_engine/ (plugin discovery).
 *
 * Discovers and loads context engine plugins from plugins/context_engine/<name>/.
 * The C equivalent is in agent/context_engine.c which provides default implementations.
 *
 * Port of Python plugins/context_engine: C handles in agent/context_engine.c
 */
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Plugin metadata ──────────────────────────────────────────── */

const char *plugin_meta_name(void) {
    return "context-engine";
}

const char *plugin_meta_version(void) {
    return "0.3.0";
}

const char *plugin_meta_type(void) {
    return "context";
}

const char *plugin_meta_description(void) {
    return "Context engine discovery — C impl in agent/context_engine.c";
}

int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ── Init ─────────────────────────────────────────────────────── */

int plugin_init(void) {
    /* context_engine_default_init() called from agent startup;
     * wires up default compressing context engine. */
    return 0;
}
