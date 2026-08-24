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

/* snapshot_registration @ ...:snapshot_registration — return the currently
 * registered provider for `name` without affecting the registry, or NULL
 * when nothing is registered under that name. (The C registry models the
 * process-global slot; there is no per-home overlay.) */
const dash_auth_provider_t *dash_auth_snapshot_registration(const char *name);

/* restore_registration @ ...:restore_registration — restore a host-owned
 * registration if it is still current. If `current` is not what is
 * registered under `name`, returns false and changes nothing. Otherwise
 * `previous == NULL` removes the registration and any non-NULL `previous`
 * replaces it. Returns true when the registry was mutated. */
bool dash_auth_restore_registration(const char *name,
                                    const dash_auth_provider_t *current,
                                    const dash_auth_provider_t *previous);

/* register_global_provider @ ...:register_global_provider — upsert a
 * host-owned provider into the process-global slot. Unlike
 * dash_auth_register_provider() this REPLACES any same-name entry instead
 * of failing, so a forced plugin re-discovery rotates the provider in
 * place. Returns true on success. */
bool dash_auth_register_global_provider(const dash_auth_provider_t *provider);

/* unregister_global_provider @ ...:unregister_global_provider — remove a
 * global registration if `provider` is still the live one under `name`.
 * Identity-conditional so a stale handle never clears a replaced
 * registration. Returns true when removed. */
bool dash_auth_unregister_global_provider(const char *name,
                                          const dash_auth_provider_t *provider);

#endif /* HERMES_DASH_AUTH_REGISTRY_H */
