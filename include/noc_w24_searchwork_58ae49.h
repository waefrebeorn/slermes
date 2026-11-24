/*
 * noc_w24_searchwork_58ae49.h - C surface for upstream
 * plugins/web/ddgs/_search_worker.py (gate w24_searchwork_58ae49).
 *
 * The child-process worker's request decision: which envelope it writes and
 * with which exit code, for a test hook or a search request.  stdin/stdout,
 * the env gate and the provider call are seams -- the provider result is a
 * fixed echo of (query, safe_limit), and the hooks that only exist to prove a
 * hung child is killable ("sleep"/"gil") answer their post-30s envelope
 * without waiting, so nothing here blocks.
 */
#ifndef W24_SEARCHWORK_58AE49_H
#define W24_SEARCHWORK_58AE49_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The env var that arms the test hooks, and the hook names. */
#define W24_SEARCHWORK_58AE49_HOOKS_ENV "HERMES_DDGS_ALLOW_TEST_HOOKS"

/* {"query": <any>, "safe_limit": <any>} -> {"query": str, "safe_limit": int}
 * as main() computes them: str(query or ""), max(1, int(safe_limit or 1)).
 * null when int() would raise (the live line is outside its try/except). */
json_t *w24_searchwork_58ae49_normalize(const json_t *req);

/* The whole decision: {"code": int, "envelope": {...}}.
 * A truthy test_hook is refused unless hooks are enabled, then "empty" answers
 * {"ok": true, "results": []}, "sleep"/"gil" answer their timed-out envelope,
 * anything else answers `unknown test_hook: <repr>`.  Otherwise the search
 * envelope carries the provider seam {"query", "limit"}. */
json_t *w24_searchwork_58ae49_handle(const json_t *req, int hooks_enabled);

/* The literal tables: hook names, the env name, the refusal/envelope texts. */
json_t *w24_searchwork_58ae49_constants(void);

#ifdef __cplusplus
}
#endif
#endif
