/*
 * url_safety_cli.h — minimal declaration surface for the pure SSRF-safety
 * helpers in src/cli/port_tools_url_safety.c (port of tools/url_safety.py).
 *
 * Opaque / minimal: no god-header, no struct exposure. Only the deterministic,
 * oracle-viable entry points are declared here.
 */

#ifndef HERMES_URL_SAFETY_CLI_H
#define HERMES_URL_SAFETY_CLI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* tools/url_safety.py:normalize_url_for_request
 * Returns 0 on success (normalized written to normalized_out, NUL-terminated),
 * -1 when the URL is not an http/https URL (normalized_out left empty). */
int cli_tools_url_safety_normalize_url_for_request(
    const char *url, char *normalized_out, size_t norm_size);

/* tools/url_safety.py:_is_blocked_ip — returns 1 (true) when the literal IP
 * string is in a blocked range, 0 otherwise. Unparseable input is blocked. */
int cli_tools_url_safety__is_blocked_ip(const char *ip_str);

/* tools/url_safety.py:is_always_blocked_url — returns 1 for cloud-metadata
 * hostnames / literal IPs, else 0. (Hostname→DNS paths are environment
 * dependent and intentionally excluded from the oracle.) */
int cli_tools_url_safety_is_always_blocked_url(const char *url);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_URL_SAFETY_CLI_H */
