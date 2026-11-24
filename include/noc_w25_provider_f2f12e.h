/*
 * noc_w25_provider_f2f12e.h - C surface for upstream plugins/web/parallel/provider.py
 * (gate w25_provider_f2f12e).
 *
 * The value-only core of the Parallel.ai web-search provider: the
 * PARALLEL_SEARCH_MODE resolver (default/env/lowercase/strip/narrow), the
 * beta.search call plan (objective, mode, max_results capped at the shared
 * SEARCH_LIMIT_CAP) and the response->web-hit shaping done inside search().
 * The SDK client, the key lookup and the keyless ring stay a seam: the SDK is a
 * fixed client object and the keyless decision is a fixed input.
 */
#ifndef NOC_W25_PROVIDER_F2F12E_H
#define NOC_W25_PROVIDER_F2F12E_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared server-side cap every bundled vendor applies to max_results. */
#define W25_PROVIDER_F2F12E_SEARCH_LIMIT_CAP 20

/* {"mode_env": <str|null>} -> "fast" | "one-shot" | "agentic".
 * os.getenv(..., "agentic").lower().strip(), narrowed to the known modes. */
json_t *w25_provider_f2f12e_resolve_search_mode(const json_t *req);

/* {"query": <any>, "limit": <num>, "mode_env": <str|null>} ->
 * {"mode": str, "max_results": num, "search_queries": [query],
 *  "objective": query} -- exactly the kwargs search() passes to
 *  client.beta.search(). */
json_t *w25_provider_f2f12e_search_plan(const json_t *req);

/* {"results": [{"url","title","excerpts"}...]} ->
 * {"success": true, "data": {"web": [{"url","title","description",
 *  "position"}...]}} -- Parallel's response.results mapped through web_hit,
 *  excerpts joined by one space, 1-based positions. */
json_t *w25_provider_f2f12e_search_hits(const json_t *req);

/* NAME/DISPLAY_NAME/KEY_ENV/EXTRACT/KEYLESS, the cap, the missing-key
 * message and the accepted modes, as an object. */
json_t *w25_provider_f2f12e_constants(void);

#ifdef __cplusplus
}
#endif
#endif
