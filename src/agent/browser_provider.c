/*
 * browser_provider.c — Browser Provider interface implementation.
 *
 * MS01: Port of Python agent/browser_provider.py (175 LOC, 1 class BrowserProvider).
 * Implements default provider methods and session lifecycle.
 */

#include "browser_provider.h"
#include <stdlib.h>
#include <string.h>

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

/*
 * N/A (Python-only):
 *   BrowserProvider.__init__() — class constructor, N/A for C struct pattern.
 *   BrowserProvider.name property — C uses direct struct member.
 *   BrowserProvider.display_name property — C uses direct struct member.
 *   BrowserProvider.is_available() — C uses function pointer in struct.
 *   BrowserProvider.create_session() — C uses function pointer in struct.
 *   BrowserProvider.close_session() — C uses function pointer in struct.
 *   BrowserProvider.emergency_cleanup() — C uses function pointer in struct.
 *   BrowserProvider.get_setup_schema() — returns Python dict, N/A for C.
 */
