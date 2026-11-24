#ifndef NOC_W20_SEARCH_DE6DEB_H
#define NOC_W20_SEARCH_DE6DEB_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/search.py (gate w20_search_de6deb).
 *
 * The store handle, the holder and the backend's search_messages() are seams:
 * the port keeps the caller-side mapping the module owns -- the sort table,
 * _optional_list, _hit's projection and the keyword set handed to the store.
 */

/* PoP: w20_search_de6deb_sort_arg @ persistence/sqlite/conversation/search.py:_SORT
 * {"value": null|"newest"|"oldest"} for relevance|newest|oldest, else
 * {"error": "KeyError"} -- `_SORT[sort]` raises on anything else. */
json_t *w20_search_de6deb_sort_arg(const char *sort_name);

/* PoP: w20_search_de6deb_optional_list @ persistence/sqlite/conversation/search.py:_optional_list
 * None -> null, otherwise list(values) (a string becomes its characters). */
json_t *w20_search_de6deb_optional_list(const json_t *values);

/* PoP: w20_search_de6deb_hit @ persistence/sqlite/conversation/search.py:_hit
 * One store match dict as the hit projection. Raises (an {status:"raised",
 * type, message} answer) for a non-int id or a context row without a role. */
json_t *w20_search_de6deb_hit(const json_t *match);

/* PoP: w20_search_de6deb_store_call @ persistence/sqlite/conversation/search.py:SQLiteSessionSearch.search_messages
 * params = {"query","source_filter","exclude_sources","role_filter","limit",
 *           "offset","sort","include_inactive","fields","after_ts","before_ts",
 *           "matches": [...]}
 * Answer: {"status":"ok","value":{"kwargs": {...}, "hits": [...]}} -- the exact
 * keyword set the method forwards to the store plus the projected hits. */
json_t *w20_search_de6deb_store_call(const json_t *params);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_SEARCH_DE6DEB_H */
