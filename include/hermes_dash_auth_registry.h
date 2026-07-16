/*
 * hermes_dash_auth_registry.h — C port of hermes_cli/dashboard_auth/registry.py
 *
 * Module-level registry for DashboardAuthProvider instances. Plugins call
 * dash_auth_register_provider() via the plugin context hook at startup.
 * The auth-gate middleware iterates dash_auth_list_providers() and uses
 * dash_auth_get_provider() to dispatch on the session's `provider` field.
 */

#ifndef HERMES_DASH_AUTH_REGISTRY_H
#define HERMES_DASH_AUTH_REGISTRY_H

#include <stddef.h>
#include <stdbool.h>

/* Minimal DashboardAuthProvider surface we track in the registry. */
typedef struct {
    const char *name;            /* provider name (registry key) */
    const char *display_name;     /* human label */
    bool         supports_token;  /* non-interactive token auth */
    bool         supports_session; /* interactive cookie sessions */
} dash_auth_provider_t;

/*
 * Register a provider. Returns true on success, false if a provider with
 * the same name is already registered (mirrors the Python ValueError).
 * Faithful to dashboard_auth.registry.register_provider.
 */
bool dash_auth_register_provider(const dash_auth_provider_t *provider);

/* Return the registered provider for `name`, or NULL if unknown. */
const dash_auth_provider_t *dash_auth_get_provider(const char *name);

/* All registered providers, in registration order (NULL-terminated array of
 * pointers into the registry; do NOT free). Caller may free the array. */
dash_auth_provider_t **dash_auth_list_providers(int *out_n);

/* Registered providers whose supports_token flag is true (registration order).
 * Returns a malloc'd NULL-terminated array of pointers. */
dash_auth_provider_t **dash_auth_list_token_providers(int *out_n);

/* Registered providers whose supports_session flag is true (registration order).
 * Returns a malloc'd NULL-terminated array of pointers. */
dash_auth_provider_t **dash_auth_list_session_providers(int *out_n);

/* Test-only: drop all registrations. Faithful to clear_providers. */
void dash_auth_clear_providers(void);

#endif /* HERMES_DASH_AUTH_REGISTRY_H */
