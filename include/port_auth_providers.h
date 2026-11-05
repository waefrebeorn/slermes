/*
 * port_auth_providers.h — pure provider-resolution helpers from
 * hermes_cli/auth_commands.py. The config/auth-store reads stay unported;
 * these take the resolved facts as parameters.
 */
#ifndef PORT_AUTH_PROVIDERS_H
#define PORT_AUTH_PROVIDERS_H

/* _provider_base_url: provider -> base URL string, from the resolved facts.
 *   openrouter         -> the fixed openrouter URL
 *   starts with custom:-> *_custom_base_url
 *   configured set     -> *_configured_base_url
 *   else               -> *_registry_base_url ("" when provider unknown)
 * Values are trimmed. Returns malloc'd string, caller frees. */
char *cpa_provider_base_url(const char *provider, const char *registry_base_url,
                            const char *configured_base_url, const char *custom_base_url);

/* _is_known_provider(provider, configured_provider): True when the provider is
 * in the registry, equals "openrouter", starts with "custom:", or has a
 * configured entry. in_registry/configured are the parameterized facts. */
int cpa_is_known_provider(const char *provider, int in_registry, int configured);

#endif /* PORT_AUTH_PROVIDERS_H */
