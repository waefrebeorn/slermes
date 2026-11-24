/*
 * noc_w24_promptrefr_587f3f.h - C surface for upstream
 * persistence/sqlite/conversation/_store/prompt_refresh.py
 * (gate w24_promptrefr_587f3f).
 *
 * Operator-requested system-prompt refresh epochs.  The SQLite handle is a
 * seam (``_execute_write`` / ``_read_one``): what is ported here is what those
 * two statements decide from the values they return -- which state_meta key a
 * request lands on, what the next epoch is, and how a MAX(...) row collapses
 * to an int.  The key predicates themselves (PROFILE_KEY / SESSION_KEY_PREFIX)
 * and the int() coercions are pure value -> value.
 */
#ifndef NOC_W24_PROMPTREFR_587F3F_H
#define NOC_W24_PROMPTREFR_587F3F_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* session_id: <any> -> string.  None -> PROFILE_KEY, otherwise
 * SESSION_KEY_PREFIX + session_id.  A non-string session_id is the
 * ``str + non-str`` TypeError seam and yields null. */
json_t *w24_promptrefr_587f3f_refresh_key(const json_t *session_id);

/* current: <any> -> number.  int(current or 0) + 1, i.e. the epoch the next
 * refresh request records.  A value int() refuses (ValueError) yields null. */
json_t *w24_promptrefr_587f3f_next_epoch(const json_t *current);

/* row: [value] | null -> number.  0 when the MAX(...) row is missing, else
 * int(row[0] or 0).  An unparsable value yields null (ValueError seam). */
json_t *w24_promptrefr_587f3f_epoch_from_row(const json_t *row);

/* row: [value] | null -> number.  0 when the COUNT(*) row is missing, else
 * int(row[0]).  None/""/text are the int() TypeError/ValueError seam -> null. */
json_t *w24_promptrefr_587f3f_count_from_row(const json_t *row);

/* The state_meta key literals, as an object. */
json_t *w24_promptrefr_587f3f_constants(void);

#ifdef __cplusplus
}
#endif
#endif
