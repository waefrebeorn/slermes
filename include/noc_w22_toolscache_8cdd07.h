#ifndef NOC_W22_TOOLSCACHE_8CDD07_H
#define NOC_W22_TOOLSCACHE_8CDD07_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tools/connectors/portal/tools_cache.py (gate w22_toolscache_8cdd07).
 *
 * `read_tools` is bound to an HTTP client, the cache directory and the clock;
 * those are the network/filesystem/clock seams. The pure decisions are here:
 *
 *   - `_member_key`: the JWT `sub` claim hashed to a 16-hex member key.
 *   - `validate_slug`: the slug grammar that gates every call.
 *   - `_cache_path`: the per-origin/per-member/per-slug cache location.
 *   - the TTL / refresh / not-modified / stale-fallback decision lattice.
 */

/* PoP: w22_toolscache_8cdd07_member_key @ tools/connectors/portal/tools_cache.py:_member_key
 * sha256(sub)[:16] for a well-formed 3-part token whose payload carries a
 * non-empty string `sub`; NULL for None, a malformed token or a bad claim
 * (the live function's None, which is not an error). */
char *w22_toolscache_8cdd07_member_key(const char *token);

/* PoP: w22_toolscache_8cdd07_slug_valid @ tools/connectors/portal/client.py:validate_slug
 * 1 when `^[a-z0-9][a-z0-9_-]*$` fullmatches (validate_slug raises otherwise). */
int w22_toolscache_8cdd07_slug_valid(const char *slug);

/* PoP: w22_toolscache_8cdd07_cache_rel @ tools/connectors/portal/tools_cache.py:_cache_path
 * The home-relative cache path: cache/connectors/<sha16(origin)>/<member>/<slug>.json */
char *w22_toolscache_8cdd07_cache_rel(const char *origin, const char *member_key,
                                      const char *slug);

/* PoP: w22_toolscache_8cdd07_home_env @ tools/connectors/portal/tools_cache.py:<module>
 * Harness isolation seam: the home the runner handed this process
 * (SLERMES_HOME, then HERMES_HOME, then HOME), or NULL when none is set.
 * Not upstream behaviour; it proves the harness never reads a real home. */
char *w22_toolscache_8cdd07_home_env(void);

/* PoP: w22_toolscache_8cdd07_read_plan @ tools/connectors/portal/tools_cache.py:read_tools
 * The outcome of one read_tools call with the client/path/clock stubbed:
 * {"source": "cache"|"network"|"stale"|"revalidated"|"raise",
 *  "stale": bool, "fetched_at": number|null, "stored": bool,
 *  "unlinked": bool, "error": string|null}
 * Fixture keys: token, origin, slug, refresh, now, cached, listing,
 * connector, toolkit_version, etag, tools. */
json_t *w22_toolscache_8cdd07_read_plan(const json_t *req);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_TOOLSCACHE_8CDD07_H */
