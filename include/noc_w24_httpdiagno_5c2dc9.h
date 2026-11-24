/*
 * noc_w24_httpdiagno_5c2dc9.h - C surface for upstream
 * gateway/relay/http_diagnostics.py (gate w24_httpdiagno_5c2dc9).
 *
 * Describing a non-2xx answer on the gateway -> connector routes: was the
 * response the connector's own refusal, or an egress proxy answering with its
 * own status?  The decision is a pure function of the response body text and
 * the response headers; the urllib.error.HTTPError object and the body read
 * are the seam.
 */
#ifndef NOC_W24_HTTPDIAGNO_5C2DC9_H
#define NOC_W24_HTTPDIAGNO_5C2DC9_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W24_HTTPDIAGNO_5C2DC9_ORIGIN_HEADERS 4
#define W24_HTTPDIAGNO_5C2DC9_PROXY_ONLY_HEADERS 3
#define W24_HTTPDIAGNO_5C2DC9_PROXY_SERVER_NAMES 8

/* body: <str> -> str.  The connector's own refusal reason (a JSON
 * {"error": <str>} body, stripped), else "".  A body that is not UTF-8 JSON
 * is a proxy page, not the connector. */
json_t *w24_httpdiagno_5c2dc9_connector_reason(const json_t *body);

/* headers: {<name>: <value>} | null -> bool.  A header only an intermediary
 * sets (Via, Proxy-Status, X-Squid-Error), or a Server value naming an
 * egress/CDN/mesh proxy, means the response is not the connector's.  Header
 * names are matched case-insensitively, as HTTPMessage.get does. */
json_t *w24_httpdiagno_5c2dc9_answered_by_proxy(const json_t *headers);

/* text, secrets: <str>, [<str>] -> str.  _scrub: every exact credential in
 * secrets replaced by "***", in order; falsy secrets are skipped. */
json_t *w24_httpdiagno_5c2dc9_scrub(const json_t *text, const json_t *secrets);

/* The header tuples, the proxy server names and the two body limits. */
json_t *w24_httpdiagno_5c2dc9_constants(void);

#ifdef __cplusplus
}
#endif
#endif
