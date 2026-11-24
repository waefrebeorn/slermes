#ifndef NOC_W20_PROVIDER_9C3993_H
#define NOC_W20_PROVIDER_9C3993_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/web/keenable/provider.py (gate w20_provider_9c3993).
 *
 * HTTP is a seam: the port builds the request the provider would send and maps
 * the scripted reply back through the same result shapes (_common's
 * search_ok/search_fail/web_hit/document/page_error), including Keenable's
 * origin-error translation.
 */

/* PoP: w20_provider_9c3993_headers @ plugins/web/keenable/provider.py:_keenable_headers
 * {"X-Keenable-Title":"hermes-agent"} plus "Authorization": "Bearer <key>" when
 * a key is present. */
json_t *w20_provider_9c3993_headers(const char *api_key);

/* PoP: w20_provider_9c3993_search_request @ plugins/web/keenable/provider.py:KeenableWebSearchProvider.search
 * The keyed POST: {"keyless":false,"url":...,"body":{"query","max_results"},
 * "headers":{...}} where max_results is int(limit) clamped to 1..20; the keyless
 * tier answers {"keyless":true}. */
json_t *w20_provider_9c3993_search_request(const char *query, double limit, int keyless,
                                            const char *api_key);

/* PoP: w20_provider_9c3993_search_result @ plugins/web/keenable/provider.py:KeenableWebSearchProvider.search
 * Map a keyed search reply: {"status":<http>,"text":<body>,"body":<json>,
 * "json_error":<str|absent>} -> search_ok([web_hit...]) | search_fail(...). */
json_t *w20_provider_9c3993_search_result(const json_t *reply);

/* PoP: w20_provider_9c3993_extract @ plugins/web/keenable/provider.py:KeenableWebSearchProvider.extract
 * `urls` plus `replies` (one scripted reply per URL: status, text, body,
 * json_error, get_error) -> the per-URL entries extract() returns. */
json_t *w20_provider_9c3993_extract(const json_t *urls, const json_t *replies);

/* PoP: w20_provider_9c3993_setup_schema @ plugins/web/keenable/provider.py:KeenableWebSearchProvider.get_setup_schema
 * keyless_variant_schema("Keenable", "KEENABLE_API_KEY", "https://keenable.ai", ...). */
json_t *w20_provider_9c3993_setup_schema(void);

/* PoP: w20_provider_9c3993_identity @ plugins/web/keenable/provider.py:KeenableWebSearchProvider
 * {"name","display_name","key_env","extract","keyless"}. */
json_t *w20_provider_9c3993_identity(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_PROVIDER_9C3993_H */
