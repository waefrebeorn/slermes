/*
 * noc_w26_provider_9eacd0.h - C surface for upstream
 * plugins/web/perplexity/provider.py (gate w26_provider_9eacd0).
 *
 * The provider's value->value core: the Search API response mapped to the
 * web_search tool shape, the snippets response mapped to extract documents, and
 * the relevance query derived from URL path words.  The httpx POST, the key
 * lookup and the managed-gateway resolution are seams; a fixture supplies the
 * body a 200 would have returned.
 */
#ifndef NOC_W26_PROVIDER_9EACD0_H
#define NOC_W26_PROVIDER_9EACD0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* response -> {"success": true, "data": {"web": [{title,url,description,position}]}} */
json_t *w26_provider_9eacd0_normalize_search_results(const json_t *response);

/* {"response": ..., "urls": [...]} -> one extract document per requested URL. */
json_t *w26_provider_9eacd0_normalize_snippets(const json_t *req);

/* {"urls": [...]} -> the relevance query (path words, <=500 chars). */
json_t *w26_provider_9eacd0_query_for_urls(const json_t *req);

/* The endpoint/limit literals, as an object. */
json_t *w26_provider_9eacd0_constants(void);

#ifdef __cplusplus
}
#endif
#endif
