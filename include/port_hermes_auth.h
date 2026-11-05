#ifndef PORT_HERMES_AUTH_H
#define PORT_HERMES_AUTH_H

/* Pure URL/key-shape predicates from Python: hermes_cli/auth.py. The env/secret
 * lookup and provider-registry constructors (_api_key_provider, get_anthropic_key,
 * has_usable_secret's resolvers) are runtime and not ported. */

#define AUTH_DEFAULT_ACTUAL_BASE_URL "https://api.actual.inc/v1"

/* urlparse(base_url).hostname lowercased/rstrip('.') in the loopback set. */
int auth_is_actual_local_base_url(const char *base_url);

/* strip/rstrip('/'), "" -> DEFAULT, loopback-or-api.actual.inc bare -> +"/v1". */
char *auth_normalize_actual_base_url(const char *base_url);

/* Python _is_placeholder_shape: your_*_here / sk-|ghp_|hf_ with x-tail /
 * stripped-of-separators all-x. */
int auth_is_placeholder_shape(const char *value);

/* startswith the provider's declared key prefix (only "openrouter" declares). */
int auth_matches_key_prefix(const char *provider_id, const char *val);

/* looks_like_openrouter_key: _matches("openrouter", str(value).strip()). */
int auth_looks_like_openrouter_key(const char *value);

#endif /* PORT_HERMES_AUTH_H */
