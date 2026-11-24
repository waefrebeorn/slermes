#ifndef NOC_W21_TODO_138ED8_H
#define NOC_W21_TODO_138ED8_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/_store/todo.py (gate w21_todo_138ed8).
 *
 * `get_latest_todo_result` walks a live sqlite transcript through a StoreHolder;
 * the sqlite handle, the row cursor and the dedupe hash of a row are the DB
 * seam. Everything the walk *decides* is pure and ported here:
 *
 *   - is_todo_call: is this assistant tool_call the Todo call a result row
 *     answers (directly, or as a `tool_call` wrapper whose inner calls name it)?
 *   - paired: does the preceding assistant row carry such a call?
 *   - valid_result: does the row's content decode to a usable Todo payload
 *     (`{"todos": [...], "revision": int}`)?
 *   - copy_ids: the copy identities of a result row (display hash + message
 *     uid + absorbed uids), as `_dedupe_display_generations(drop_removed=True)`
 *     sees them.
 *   - removed_map / copy_removed: the newest removed copy per identity in the
 *     lineage, and whether a candidate row is shadowed by one.
 *
 * Copy identities travel as rendered strings: the display hash verbatim, and
 * every `("uid", uid)` pair as "uid:<uid>". `identity` (the display dedupe
 * hash) is supplied by the caller that owns the store's hashing.
 */

/* PoP: w21_todo_138ed8_is_todo_call @ persistence/sqlite/conversation/_store/todo.py:is_todo_call
 * 1 when call answers `call_id`, 0 when it does not, -1 when the live call
 * raises (an `arguments` string that is not JSON) -- which the caller's
 * `except (ValueError, TypeError)` turns into a skipped row. */
int w21_todo_138ed8_is_todo_call(const json_t *call, const char *call_id);

/* PoP: w21_todo_138ed8_paired @ persistence/sqlite/conversation/_store/todo.py:get_latest_todo_result
 * `isinstance(calls, list) and any(is_todo_call(c) for c in calls)` over the
 * decoded `prior["tool_calls"]` text; a decode raise (or a raise inside the
 * predicate) skips the row, so it reports 0. Short-circuits in order. */
int w21_todo_138ed8_paired(const char *tool_calls_json, const char *call_id);

/* PoP: w21_todo_138ed8_valid_result @ persistence/sqlite/conversation/_store/todo.py:get_latest_todo_result
 * The validity gate applied to a candidate row's content: decodes to a dict, a
 * list `todos`, and `int(revision or 0)` does not raise. */
int w21_todo_138ed8_valid_result(const char *content);

/* PoP: w21_todo_138ed8_copy_ids @ persistence/sqlite/conversation/_store/todo.py:_todo_copies
 * Rendered copy identities, in order: the display hash, then one "uid:<uid>"
 * per non-empty, de-duplicated message uid (message_uid first, then the
 * absorbed list, which may be the JSON text an import carries). */
json_t *w21_todo_138ed8_copy_ids(const json_t *identity, const char *message_uid,
                                 const json_t *absorbed);

/* PoP: w21_todo_138ed8_removed_map @ persistence/sqlite/conversation/_store/todo.py:_removed_todo_copies
 * rows: [{"id": n, "display_identity": s, "message_uid": s,
 *         "absorbed_message_uids": ...}]
 * -> {"<copy>": newest removed row id}. The newest id per copy wins; ids
 * are int64 and travel as decimal strings (a number is accepted too). */
json_t *w21_todo_138ed8_removed_map(const json_t *rows);

/* PoP: w21_todo_138ed8_copy_removed @ persistence/sqlite/conversation/_store/todo.py:_todo_copy_removed
 * `bool(removed) and any(removed.get(copy, 0) > row_id for copy in copy_ids)`.
 * row_id and the map's ids are int64 decimal strings or numbers. */
int w21_todo_138ed8_copy_removed(const json_t *removed, const json_t *copy_ids,
                                 const json_t *row_id);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_TODO_138ED8_H */
