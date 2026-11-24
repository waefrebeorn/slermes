#ifndef NOC_W22_GATEWAY_61B987_H
#define NOC_W22_GATEWAY_61B987_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/_store/gateway.py (gate
 * w22_gateway_61b987).
 *
 * The module's only function is a listing reader: it drains async accounting
 * deltas (`holder.flush_for_read()`), composes the session query from the
 * `platform` / `active_only` switches, and maps rows through
 * `_session_row_dict`.  Row mapping lives in core.py (a different C home), so
 * this port exposes the query/parameter decision plus the flush it performed;
 * the `_read_all` seam itself stays upstream behind the holder.
 */

/* PoP: w22_gateway_61b987_sessions_query @ persistence/sqlite/conversation/_store/gateway.py:list_gateway_sessions
 *
 * `platform` is the raw wire value (`null`/absent for None).  A truthy value is
 * bound as the single `?` of `LOWER(source) = LOWER(?)`; falsy values (None,
 * "", 0, false) drop both the clause and the parameter, exactly like
 * `[platform] if platform else []`.
 *
 * `active_only` is the raw wire value; absent arrives as NULL and Python's
 * default (True) applies.  Any falsy value drops `AND ended_at IS NULL`.
 *
 * Answer: {"sql": <composed statement>, "params": [...], "flushed": 1}.
 * The trailing `" ORDER BY last_active DESC"` is unconditional. */
json_t *w22_gateway_61b987_sessions_query(const json_t *platform,
                                          const json_t *active_only);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_GATEWAY_61B987_H */
