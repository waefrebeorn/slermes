#ifndef NOC_W21_COMPACTION_8A06A1_H
#define NOC_W21_COMPACTION_8A06A1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/_store/compaction_display.py (gate
 * w21_compaction_8a06a1): display reads paint a compacted tool result once, as
 * its archived original.
 *
 * The module is pure text and rows: `not_a_later_tool_copy_sql` composes a SQL
 * filter from `_sql_json_extract` (persistence/sqlite/_schema_ddl.py) and
 * `MODEL_ONLY_DISPLAY_METADATA_KEY` (session_rules/turns.py), and
 * `later_tool_copy_ids` is its Python twin over the display set's rows. The
 * display-metadata reader `_is_model_only_row` is ported with it: its `holder`
 * argument is never touched, so the row alone answers. */

/* PoP: w21_compaction_8a06a1_not_a_later_tool_copy_sql @ persistence/sqlite/conversation/_store/compaction_display.py:not_a_later_tool_copy_sql
 * "Filter that drops *alias* rows that are a later copy of an archived tool
 * result": the exact SQL text the query builder splices in. Caller frees. */
char *w21_compaction_8a06a1_not_a_later_tool_copy_sql(const char *alias);

/* PoP: w21_compaction_8a06a1_later_tool_copy_ids @ persistence/sqlite/conversation/_store/compaction_display.py:later_tool_copy_ids
 * The ids of *rows* (the display set) that the SQL filter above would skip.
 * Empty when the rows carry no archive flag (`compacted`) or no slot
 * (`display_order`). Python returns a set of ids; the port returns them
 * ascending, so the wire form is stable. Row fields read: id, role,
 * session_id, message_uid, timestamp, active, compacted, display_order,
 * display_metadata. Missing optional fields read as None. */
json_t *w21_compaction_8a06a1_later_tool_copy_ids(const json_t *rows);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_COMPACTION_8A06A1_H */
