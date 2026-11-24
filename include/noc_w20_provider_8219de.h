#ifndef NOC_W20_PROVIDER_8219DE_H
#define NOC_W20_PROVIDER_8219DE_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/web/exa/provider.py (gate w20_provider_8219de).
 *
 * Only the request/response shaping and the keyless/tier decision are exposed:
 * the Exa SDK client, the network and the env lookup stay in Python.
 */

/* PoP: w20_provider_8219de_search @ plugins/web/exa/provider.py:ExaWebSearchProvider.search
 * search_ok([web_hit(r.url or "", r.title or "", " ".join(r.highlights or []), i + 1)])
 * over the SDK response's `results`: {"success":true,"data":{"web":[...]}}. */
json_t *w20_provider_8219de_search(const json_t *results);

/* PoP: w20_provider_8219de_extract @ plugins/web/exa/provider.py:ExaWebSearchProvider.extract
 * [document(r.url or "", r.title or "", r.text or "") ...] plus a
 * vendor_error_entry per `statuses[]` entry whose status == "error". */
json_t *w20_provider_8219de_extract(const json_t *results, const json_t *statuses);

/* PoP: w20_provider_8219de_crawl_status @ plugins/web/exa/provider.py:_crawl_status
 * The origin status of a failed statuses[] entry: `http_status_code`, else the
 * one its tag names (CRAWL_HTTP_<code> -> code, CRAWL_NOT_FOUND -> 404).
 * Number or null. */
json_t *w20_provider_8219de_crawl_status(const json_t *error);

/* PoP: w20_provider_8219de_vendor_error_entry @ plugins/web/_common.py:vendor_error_entry
 * Per-URL failure from the vendor's own error list, with the metadata wrapper. */
json_t *w20_provider_8219de_vendor_error_entry(const json_t *url, const json_t *status,
                                               const json_t *reason, const json_t *body);

/* PoP: w20_provider_8219de_document @ plugins/web/_common.py:document
 * Successful extract entry; raw_content mirrors content. */
json_t *w20_provider_8219de_document(const char *url, const char *title, const char *content);

/* PoP: w20_provider_8219de_setup_schema @ plugins/web/exa/provider.py:ExaWebSearchProvider.get_setup_schema
 * The picker entry for the keyless ring with its paid variant. */
json_t *w20_provider_8219de_setup_schema(void);

/* PoP: w20_provider_8219de_use_keyless @ plugins/web/keyless_mcp.py:use_keyless
 * tier in ("free","paid") -> tier == "free"; else `not api_key and enabled`. */
int w20_provider_8219de_use_keyless(const json_t *tier, const char *api_key, int enabled);

/* PoP: w20_provider_8219de_missing_key @ plugins/web/exa/provider.py:_MISSING_KEY */
const char *w20_provider_8219de_missing_key(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_PROVIDER_8219DE_H */
