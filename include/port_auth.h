/* port_auth.h -- pure helpers ported from hermes_cli/auth.py. */
#ifndef SLERMES_PORT_AUTH_H
#define SLERMES_PORT_AUTH_H

#include <stddef.h>

/* _nonempty_str: True for a non-None str with non-whitespace content. */
int st_nonempty_str(const char *value);

/* _is_placeholder_shape: True for the placeholder shapes shipped in
 * .env.example and the docs (your_*_here, sk-xxxx, ghp_xxx, hf_xxx, all-x). */
int st_is_placeholder_shape(const char *value);

/* has_usable_secret: True when a configured secret looks usable, not
 * empty/placeholder. *value* is NULL for a non-str. */
int st_has_usable_secret(const char *value, int min_length);

/* _matches_key_prefix: True when *val* starts with one of *provider_id*'s
 * declared prefixes. Only openrouter declares one; the rest are fail-open. */
int st_matches_key_prefix(const char *provider_id, const char *val);

/* looks_like_openrouter_key: only a value shaped like an OpenRouter key may be
 * read as one; a real OpenAI key must never be routed to openrouter.ai. */
int st_looks_like_openrouter_key(const char *value);

/* _usable_declared_secret: *value* stripped when it is usable and prefix-valid.
 * Returns 1 and fills *out*; 0 otherwise. A provable prefix mismatch sets
 * *warned (the logger.warning seam). */
int st_usable_declared_secret(const char *provider_id, const char *value,
                              const char *source, char *out, size_t cap,
                              int *warned);

/* is_actual_local_base_url: True for Actual's loopback local API endpoint. */
int st_is_actual_local_base_url(const char *base_url);

/* normalize_actual_base_url: append /v1 for the hosted host or a bare loopback
 * endpoint. *default_url* is the empty-input fallback (a seam, not a literal). */
void st_normalize_actual_base_url(const char *base_url, const char *default_url,
                                  char *out, size_t cap);

#endif /* SLERMES_PORT_AUTH_H */
