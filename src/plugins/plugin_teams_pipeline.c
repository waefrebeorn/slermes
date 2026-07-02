/*
 * plugin_teams_pipeline.c — Teams meeting pipeline plugin.
 * Port of Python plugins/teams_pipeline/ (meeting summarization).
 *
 * Registers CLI command for the Microsoft Teams meeting pipeline:
 * job listing, run inspection, replay, Graph validation, subscriptions.
 * The C equivalent is in src/gateway/platforms/msgraph_webhook.c.
 *
 * Port of Python plugins/teams_pipeline: C handles in gateway/platforms/msgraph_webhook.c
 */
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Plugin metadata ──────────────────────────────────────────── */

const char *plugin_meta_name(void) {
    return "teams-pipeline";
}

const char *plugin_meta_version(void) {
    return "0.3.0";
}

const char *plugin_meta_type(void) {
    return "skill";
}

const char *plugin_meta_description(void) {
    return "Teams meeting summarization pipeline — C impl in msgraph_webhook.c";
}

int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ── Init ─────────────────────────────────────────────────────── */

int plugin_init(void) {
    /* msgraph_webhook lifecycle handled by gateway_lifecycle. */
    return 0;
}
