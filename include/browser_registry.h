/*
 * browser_registry.h — Browser Provider Registry API for Hermes C.
 *
 * MS02: Port of Python agent/browser_registry.py.
 * Public API for registering and resolving cloud browser providers.
 */

#ifndef BROWSER_REGISTRY_H
#define BROWSER_REGISTRY_H

#include "browser_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Register a cloud browser provider. Re-registration overwrites.
 * Returns 0 on success, -1 on error (invalid provider or registry full). */
int browser_registry_register(browser_provider_t *provider);

/* Look up a provider by name. Returns NULL if not found. */
const browser_provider_t *browser_registry_get(const char *name);

/* List all registered providers. Returns count, sets *out_list to array. */
int browser_registry_list(const browser_provider_t ***out_list);

/* Resolve the active browser provider based on config.
 * configured: value of browser.cloud_provider from config (NULL if unset).
 * Returns NULL if no provider matches (dispatcher falls back to local mode). */
const browser_provider_t *browser_registry_resolve(const char *configured);

/* Clear all registrations. Test-only. */
void browser_registry_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BROWSER_REGISTRY_H */
