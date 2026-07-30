/*
 * browser_provider.h — Browser Provider interface for Hermes C.
 *
 * MS01: Port of Python agent/browser_provider.py (175 LOC).
 * Defines the pluggable-backend interface for cloud browser providers
 * (Browserbase, Browser Use, Firecrawl, ...).
 *
 * Providers register via browser_registry_register(); the active one
 * (selected via config) services cloud-mode browser_* tool calls.
 */

#ifndef BROWSER_PROVIDER_H
#define BROWSER_PROVIDER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct browser_provider_t browser_provider_t;
typedef struct browser_session_t browser_session_t;

/* ── Session metadata ──────────────────────────────────────────────
 * Mirrors the Python session metadata contract:
 *   { session_name, bb_session_id, cdp_url, features, external_call_id }
 * bb_session_id is a legacy key name kept for backward compat. */

struct browser_session_t {
    char session_name[256];     /* unique name for agent-browser --session */
    char bb_session_id[256];    /* provider session ID (for close/cleanup) */
    char cdp_url[1024];         /* CDP websocket URL */
    char features[2048];        /* JSON object: feature flags enabled */
    char external_call_id[256]; /* optional, managed-gateway billing key */
};

/* ── Provider interface (ABC equivalent) ──────────────────────────
 * Each provider implements these function pointers.
 * Mirrors Python BrowserProvider ABC. */

struct browser_provider_t {
    /* Stable short identifier (lowercase, hyphens).
     * Examples: "browserbase", "browser-use", "firecrawl". */
    const char *name;

    /* Human-readable label shown in hermes tools. Defaults to name. */
    const char *display_name;

    /* Return true when this provider can service calls.
     * Cheap check (env var present, token readable). No network calls. */
    bool (*is_available)(const browser_provider_t *self);

    /* Create a cloud browser session. Returns session metadata.
     * Caller must free the returned session with browser_session_free(). */
    browser_session_t *(*create_session)(const browser_provider_t *self,
                                          const char *task_id);

    /* Release/terminate a cloud session by its provider session ID.
     * Returns true on success. Must not raise. */
    bool (*close_session)(const browser_provider_t *self,
                           const char *session_id);

    /* Best-effort session teardown during process exit.
     * Must tolerate missing credentials, network errors. Must not raise. */
    void (*emergency_cleanup)(const browser_provider_t *self,
                               const char *session_id);

    /* Backward-compat aliases */
    bool (*is_configured)(const browser_provider_t *self); /* alias for is_available */
    const char *(*provider_name)(const browser_provider_t *self); /* alias for display_name */

    /* Opaque provider-specific data */
    void *priv;
};

/* ── Session lifecycle ──────────────────────────────────────────── */

/* Free a browser_session_t created by create_session(). */
void browser_session_free(browser_session_t *session);

/* ── Default implementations ────────────────────────────────────── */

/* Default is_configured() — calls is_available(). */
bool browser_provider_is_configured_default(const browser_provider_t *self);

/* Default provider_name() — returns display_name. */
const char *browser_provider_name_default(const browser_provider_t *self);

#ifdef __cplusplus
}
#endif

#endif /* BROWSER_PROVIDER_H */
