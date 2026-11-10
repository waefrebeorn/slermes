/*
 * port_agent_provider.h — pure helpers from agent/provider_registry.py.
 *
 * lower_key and the provider-name mapping half of configured_provider_name
 * are portable; the config.yaml read in configured_provider_name is runtime
 * IO, taken here as the already-read <section>.provider value. (This is a
 * different module from the provider *catalog* in port_provider_registry.h.)
 */
#ifndef PORT_AGENT_PROVIDER_H
#define PORT_AGENT_PROVIDER_H

/* provider_registry.lower_key(name): name.strip().lower(). NULL is treated as
 * "" (Python would raise on None; the C string contract maps NULL to empty).
 * Returns a malloc'd result, caller frees. */
char *ap_lower_key(const char *name);

/* provider_registry.configured_provider_name, config-read half: the caller
 * passes the raw <section>.provider value (or NULL). Returns NULL when it has
 * no non-whitespace content, a malloc'd "fal" when that value is the
 * Nous-managed provider ("nous", case-insensitive), else a malloc'd copy of
 * the stripped value. Caller frees. */
char *ap_configured_provider_name(const char *provider_value);

#endif /* PORT_AGENT_PROVIDER_H */
