/*
 * url_safety_helpers.h — public API for the pure tools/url_safety.py helpers.
 * Opaque, minimal includes.
 */

#ifndef URL_SAFETY_HELPERS_H
#define URL_SAFETY_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

/* Trusted host allowed to resolve to private/benchmark IPs over https.
 * (PoP: _allows_private_ip_resolution) */
bool tools_url_safety_allows_private_ip_resolution(const char *hostname, const char *scheme);

/* Return a malloc'd copy of the first sensitive query-param NAME (percent-
 * decoded, matching Python's unquote) if url carries one with a non-empty
 * value; otherwise NULL. Only http/https URLs are considered. Caller frees.
 * (PoP: sensitive_query_param_name) */
char *tools_url_safety_sensitive_query_param_name(const char *url);

/* True when url carries a credential-bearing query param. (PoP: has_sensitive_query_params) */
bool tools_url_safety_has_sensitive_query_params(const char *url);

#endif /* URL_SAFETY_HELPERS_H */
