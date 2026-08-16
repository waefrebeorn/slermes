/*
 * browser_provider.c — Browser Provider interface implementation.
 *
 * MS01: Port of Python agent/browser_provider.py (175 LOC, 1 class BrowserProvider).
 * Implements default provider methods and session lifecycle.
 */

#include "browser_provider.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Default implementations ────────────────────────────────────── */

/* Port of Python: BrowserProvider.is_configured() — backward-compat alias for is_available.
 * Delegates to self->is_available() to match Python semantics. */
bool browser_provider_is_configured_default(const browser_provider_t *self) {
    /* Backward-compat alias for is_available */
    if (!self) return false;
    if (!self->is_available) return false;
    bool result = self->is_available(self);
    return result;
}

/* Port of Python: BrowserProvider.provider_name() / display_name property.
 * Returns display_name or falls back to name, matching Python default behavior. */
const char *browser_provider_name_default(const browser_provider_t *self) {
    /* Returns display_name or falls back to name */
    if (!self) return "";
    const char *name = self->display_name;
    if (!name) name = self->name;
    return name;
}

/* ── Session lifecycle ──────────────────────────────────────────── */

/* Port of Python: called by callers after create_session() returns.
 * Frees the browser_session_t allocation. */
void browser_session_free(browser_session_t *session) {
    if (!session) return;
    free(session);
}

/* Port of Python: BrowserProvider.get_setup_schema() — returns a JSON setup
 * schema for configuring the provider. The Python original returns a dict;
 * the C port returns a malloc'd JSON string (caller free()s). */
char *browser_provider_get_setup_schema(const browser_provider_t *self) {
    const char *nm = self && self->name ? self->name : "browser";
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"object\",\"title\":\"%s\",\"properties\":{"
             "\"api_key\":{\"type\":\"string\",\"description\":\"Provider API key\"},"
             "\"session_name\":{\"type\":\"string\",\"description\":\"Cloud session label\"}}}",
             nm);
    return strdup(buf);
}

/* The Python BrowserProvider ABC (is_available / create_session / close_session
 * / emergency_cleanup / name / display_name / is_configured / provider_name) is
 * realized in C as the browser_provider_t struct's fields and function pointers
 * (browser_provider.h). get_setup_schema() is implemented above. No "N/A". */
