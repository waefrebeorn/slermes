/*
 * noc_w24_client_64e0a3.h - C surface for upstream
 * plugins/config_backends/remote/client.py (gate w24_client_64e0a3).
 *
 * The config-plane HTTP client. The request itself (urllib, TLS, the bearer
 * store, the destination policy) is a seam; what is ported here is the pure
 * value work around it: the response body parser, the Retry-After reader, the
 * origin/instance resolution off the environment, and the two Response
 * properties every caller keys its messages off.
 *
 * Note: libjson stores every number as a double, so a JSON number is read as a
 * Python int when it is integral -- fixtures avoid writing "5.0" where the
 * upstream value is an int.
 */
#ifndef W24_CLIENT_64E0A3_H
#define W24_CLIENT_64E0A3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {text} (the response body as decoded UTF-8) -> the body dict, {} when
 * empty, unparseable, or not a JSON object. */
json_t *w24_client_64e0a3_parse_body(const json_t *req);

/* req: {value} (the Retry-After header text) -> float or null. */
json_t *w24_client_64e0a3_retry_after(const json_t *req);

/* req: {env} (the value of HERMES_CONFIG_REMOTE_URL; null/absent = unset) ->
 * the stripped origin without a trailing slash. */
json_t *w24_client_64e0a3_base_url(const json_t *req);

/* req: {env} (HERMES_CONFIG_INSTANCE_ID) -> the stripped instance id. */
json_t *w24_client_64e0a3_instance_id(const json_t *req);

/* req: {status, body} -> str(body["error"]) or "http_<status>". */
json_t *w24_client_64e0a3_response_error(const json_t *req);

/* req: {status, body} -> str(body["message"]) or "HTTP <status>". */
json_t *w24_client_64e0a3_response_message(const json_t *req);

/* DEFAULT_URL / URL_ENV / INSTANCE_ENV / _HTTP_TIMEOUT_S. */
json_t *w24_client_64e0a3_constants(void);

#ifdef __cplusplus
}
#endif
#endif
