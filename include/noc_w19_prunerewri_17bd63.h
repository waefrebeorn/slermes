/* noc_w19_prunerewri_17bd63.h - public C API for persistence/sqlite/conversation/_store/prune_rewrite.py port. */
#ifndef NOC_W19_PRUNEREWRI_17BD63_H
#define NOC_W19_PRUNEREWRI_17BD63_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_prunerewri_17bd63_exact_id @ persistence/sqlite/conversation/_store/prune_rewrite.py:held_row_mismatches
 * The message's exact row id: an int (never a bool) above zero, else -1 (None). */
long long w19_prunerewri_17bd63_exact_id(json_t *message);

/* PoP: w19_prunerewri_17bd63_call_ids @ persistence/sqlite/conversation/_store/prune_rewrite.py:rewrite_pruned_rows
 * The id of each parsed tool call (None for a non-dict call), or an empty list for no calls. */
json_t *w19_prunerewri_17bd63_call_ids(json_t *tool_calls);

/* PoP: w19_prunerewri_17bd63_held_row_mismatches @ persistence/sqlite/conversation/_store/prune_rewrite.py:held_row_mismatches
 * The ids among `held` whose live ACTIVE row no longer holds the same payload, read against the
 * rows the connection would return (stored_rows: the messages rows of this session). Absent or
 * inactive rows are not reported; a CAS snapshot digest wins over the loader-lens identity compare. */
json_t *w19_prunerewri_17bd63_held_row_mismatches(const char *session_id, json_t *held,
                                                 json_t *stored_rows);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_PRUNEREWRI_17BD63_H */
