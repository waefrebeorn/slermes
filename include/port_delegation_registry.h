/*
 * port_delegation_registry.h — pure helpers from agent/delegation_registry.py.
 *
 * The plugin-discovery / provider-availability chain is runtime state, taken
 * here as booleans; the set-membership check and the user-facing backend_status
 * reason strings are the portable core. (configured_backend already lives in
 * port_hermes_cli_misc.c.)
 */
#ifndef PORT_DELEGATION_REGISTRY_H
#define PORT_DELEGATION_REGISTRY_H

#include <stdbool.h>

/* delegation_registry._bundled_disabled: `{name, "delegation/name"} & disabled`.
 * disabled_list is a whitespace/comma-separated list of disabled plugin names
 * (or NULL / "" for none). Returns true when the strategy is disabled. */
bool dr_bundled_disabled(const char *name, const char *disabled_list);

/* delegation_registry.backend_status, message selection. `available` is true
 * when some provider for `name` can run here (the _usable() outcome);
 * `registered` is true when a provider entry is registered under `name`.
 * Returns a malloc'd "" (available) or the exact user-facing reason string
 * (the delegate_task tool error). Caller frees. */
char *dr_backend_status_reason(const char *name, bool available,
                               bool registered);

#endif /* PORT_DELEGATION_REGISTRY_H */
