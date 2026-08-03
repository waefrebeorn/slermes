/*
 * port_provider_registry.h — Faithful C11 port of the Hermes provider catalog
 * and runtime-provider resolution helpers.
 *
 * Ports:
 *   hermes_cli/auth.py:PROVIDER_REGISTRY (34 built-in providers) + ProviderConfig
 *   hermes_cli/auth.py:resolve_provider (alias map + auto precedence chain)
 *   hermes_cli/runtime_provider.py pure helpers:
 *     _normalize_custom_provider_name, _loopback_hostname,
 *     _detect_api_mode_for_url, _host_derived_api_key,
 *     _anthropic_base_url_override_ok, _parse_api_mode,
 *     _provider_supports_explicit_api_mode, resolve_requested_provider
 *   utils.py:base_url_hostname, base_url_host_matches
 */
#ifndef PORT_PROVIDER_REGISTRY_H
#define PORT_PROVIDER_REGISTRY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One built-in provider entry (ProviderConfig). String fields are static
 * literals owned by the catalog; never freed. api_key_env_vars is a
 * NULL-terminated array of static literals (may be NULL for none). */
typedef struct {
    const char *id;
    const char *name;
    const char *auth_type;          /* "api_key" | "oauth_device_code" | ... */
    const char *portal_base_url;
    const char *inference_base_url;
    const char *client_id;
    const char *scope;
    const char *base_url_env_var;
    const char *const *api_key_env_vars;   /* NULL-terminated, or NULL */
} catalog_provider_t;

/* Look up a built-in provider by exact id. Returns a borrowed pointer into the
 * static catalog, or NULL when the id is not a known built-in. */
const catalog_provider_t *provider_registry_get(const char *id);

/* Number of built-in providers in the catalog. */
int provider_registry_count(void);

/* Borrowed pointer to entry i (0-based, in declaration order), or NULL. */
const catalog_provider_t *provider_registry_at(int i);

/* Faithful port of utils.base_url_hostname: lowercased hostname of a base URL,
 * trailing dot stripped. Returns a malloc'd string ("" when absent). */
char *provider_base_url_hostname(const char *base_url);

/* Faithful port of utils.base_url_host_matches: true when the URL's hostname
 * equals domain or is a subdomain of it. */
bool provider_base_url_host_matches(const char *base_url, const char *domain);

/* runtime_provider.py:_normalize_custom_provider_name — strip/lower/space→dash.
 * Returns malloc'd string. */
char *provider_normalize_custom_name(const char *value);

/* runtime_provider.py:_loopback_hostname — localhost/127.0.0.1/::1/0.0.0.0. */
bool provider_loopback_hostname(const char *host);

/* runtime_provider.py:_parse_api_mode — validate against the known api_mode
 * set. Returns a static literal (the canonical mode) or NULL when invalid. */
const char *provider_parse_api_mode(const char *raw);

/* runtime_provider.py:_provider_supports_explicit_api_mode */
bool provider_supports_explicit_api_mode(const char *provider,
                                         const char *configured_provider);

/* runtime_provider.py:_detect_api_mode_for_url — auto-detect api_mode from the
 * resolved base URL. Returns a static literal or NULL. */
const char *provider_detect_api_mode_for_url(const char *base_url);

/* runtime_provider.py:_anthropic_base_url_override_ok */
bool provider_anthropic_base_url_override_ok(const char *base_url);

/* runtime_provider.py:_host_derived_api_key — derive <VENDOR>_API_KEY from the
 * base URL host and read it from the environment. Returns a malloc'd string
 * ("" when no usable vendor label / not set). */
char *provider_host_derived_api_key(const char *base_url);

/* auth.py:resolve_provider — normalize a requested provider through the alias
 * map and validate against the catalog. This ports the *deterministic* part:
 * alias resolution + "openrouter"/"custom" passthrough + registry membership.
 * Returns a malloc'd canonical provider id. For an unknown non-auto provider,
 * returns a malloc'd copy of the normalized input (caller treats as error);
 * "auto" is returned as-is when no static rule applies (the env/OAuth
 * precedence chain is resolved by resolve_requested_provider upstream). */
char *provider_resolve_alias(const char *requested);

/* runtime_provider.py:_normalize_base_url_for_match — strip/rstrip-slash/lower.
 * Returns malloc'd string. */
char *provider_normalize_base_url_for_match(const char *value);

/* runtime_provider.py:find_custom_provider_identity — reverse-lookup an endpoint
 * URL to its canonical "custom:<name>" menu key from config providers /
 * custom_providers. Returns a malloc'd "custom:<name>" or NULL. */
char *provider_find_custom_identity_by_url(const char *base_url);

/* runtime_provider.py:find_custom_provider_identity_by_model — reverse-lookup a
 * model id to the "custom:<name>" entry that serves it. Returns malloc'd or NULL. */
char *provider_find_custom_identity_by_model(const char *model);

/* runtime_provider.py:canonical_custom_identity — recover a routable
 * "custom:<name>" identity for a bare "custom" provider. Priority: base_url →
 * model → config_provider. Any arg may be NULL. Returns malloc'd or NULL. */
char *provider_canonical_custom_identity(const char *base_url,
                                         const char *config_provider,
                                         const char *model);

#ifdef __cplusplus
}
#endif

#endif /* PORT_PROVIDER_REGISTRY_H */
