/*
 * noc_w24_provider_4aa510.h - C surface for upstream
 * plugins/web/tavily/provider.py (gate w24_provider_4aa510).
 *
 * The Tavily provider's value layer: the request headers for a keyed or opt-in
 * keyless call, the /search payload, the two response normalizers that map
 * Tavily's JSON onto the shared result shapes (key order is part of that
 * contract), the missing-key text, the auth triple and the `hermes tools`
 * setup entry.
 *
 * The HTTP client is a seam: ``_tavily_request`` is not ported, so nothing here
 * opens a socket.  ``provider_env`` / ``use_keyless`` also arrive as values --
 * the API key is an argument and the keyless verdict is a flag.
 */
#ifndef W24_PROVIDER_4AA510_H
#define W24_PROVIDER_4AA510_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _tavily_headers(api_key) -> {"X-Client-Name", "Authorization"|"X-Tavily-Access-Mode"}. */
json_t *w24_provider_4aa510_headers(const json_t *api_key);

/* {"query", "max_results": min(limit, 20), "include_raw_content": false,
 *  "include_images": false}; {"error":"TypeError"} when limit is uncomparable. */
json_t *w24_provider_4aa510_search_payload(const json_t *query, const json_t *limit);

/* _normalize_tavily_search_results(response) ->
 * {"success":true,"data":{"web":[{"title","url","description","position"}]}};
 * {"error":<type>} for the shapes the source raises on. */
json_t *w24_provider_4aa510_normalize_search(const json_t *response);

/* _normalize_tavily_documents(response) -> the successful documents followed by
 * the failed_results / failed_urls entries, each with sourceURL metadata. */
json_t *w24_provider_4aa510_normalize_documents(const json_t *response);

/* _missing_key_error(action). */
json_t *w24_provider_4aa510_missing_key_error(const json_t *action);

/* _auth(api_key, keyless, action) -> {"key","missing","prefix"}: key is "" when
 * keyless is forced, null when neither a key nor keyless applies. */
json_t *w24_provider_4aa510_auth(const json_t *api_key, const json_t *keyless,
                                 const json_t *action);

/* get_setup_schema() -> the `hermes tools` picker entry. */
json_t *w24_provider_4aa510_setup_schema(void);

/* NAME / DISPLAY_NAME / KEY_ENV / EXTRACT / KEYLESS / _CLIENT_NAME /
 * _SEARCH_PAYLOAD / SEARCH_LIMIT_CAP, as an object. */
json_t *w24_provider_4aa510_constants(void);

#ifdef __cplusplus
}
#endif
#endif
