/*
 * plugin_dashboard_auth.c — Dashboard auth plugin.
 * Port of Python plugins/dashboard_auth/ (auth middleware).
 *
 * Provides authentication for the web dashboard (nous, basic, self_hosted).
 * The C equivalent is in web_dashboard.c.
 *
 * Port of Python plugins/dashboard_auth: C handles in web_dashboard.c
 */
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Plugin metadata ──────────────────────────────────────────── */

const char *plugin_meta_name(void) {
    return "dashboard-auth";
}

const char *plugin_meta_version(void) {
    return "0.3.0";
}

const char *plugin_meta_type(void) {
    return "skill";
}

const char *plugin_meta_description(void) {
    return "Dashboard authentication (nous, basic, self_hosted) — C impl in web_dashboard.c";
}

int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ── Init ─────────────────────────────────────────────────────── */

int plugin_init(void) {
    /* web_dashboard_init() called from main.c; sets up dashboard auth. */
    return 0;
}
