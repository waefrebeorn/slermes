/*
 * noc_w21_sessionlis_08fd81.h -- public C API for the port of upstream
 *   persistence/sqlite/conversation/_store/session_list.py
 *
 * The module body is one pure projection: compression roots surface their live
 * tip's fields.  The two DB reads it performs (`get_compression_chain` and
 * `_get_session_rich_rows_batch`) are seams: the caller hands the C port the
 * chain map and the tip-row map it already fetched, exactly as the Python code
 * holds them in `chain_by_root` / `tip_rows` after its batched query.
 */
#ifndef NOC_W21_SESSIONLIS_08FD81_H
#define NOC_W21_SESSIONLIS_08FD81_H

#include "hermes_json.h"

/*
 * project_compression_tips(sessions, chains, tip_rows) -> array
 *
 *   sessions : array of session row objects (the page being projected)
 *   chains   : object  root_id -> array of chain ids (get_compression_chain)
 *   tip_rows : object  tip_id  -> rich row object (_get_session_rich_rows_batch)
 *
 * Returns the projected array.  A missing (None) `sessions` or a non-object
 * session row yields the same exception shape Python raises:
 *   {"status":"error","type":"TypeError"|"AttributeError"|"KeyError","message":...}
 */
json_t *w21_sessionlis_08fd81_project_compression_tips(const json_t *sessions,
                                                       const json_t *chains,
                                                       const json_t *tip_rows);

#endif /* NOC_W21_SESSIONLIS_08FD81_H */
