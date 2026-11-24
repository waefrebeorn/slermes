#ifndef NOC_W20_HOSTEDROOM_B5ADC0_H
#define NOC_W20_HOSTEDROOM_B5ADC0_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/hosted_rooms_legacy_import.py (gate w20_hostedroom_b5adc0).
 *
 * The one-shot import of the pre-isolation hosted_room* rows. The pure
 * decisions (predecessor path, settle gate, per-table copy plan and the
 * (target column, source expression) pairs) are exposed here; the sqlite
 * connections, the savepoint and the marker write stay in Python.
 */

/* PoP: w20_hostedroom_b5adc0_source_path @ gateway/hosted_rooms_legacy_import.py:source_path
 * `db_path.with_name("state.db")` when the name is "shared-state.db", else null. */
json_t *w20_hostedroom_b5adc0_source_path(const char *db_path);

/* PoP: w20_hostedroom_b5adc0_settled @ gateway/hosted_rooms_legacy_import.py:settled
 * True once the import is recorded, given up on for this process, or never
 * applies: `source is None or source in _failed_sources or (table_exists and
 * the marker row for the source exists)`. */
json_t *w20_hostedroom_b5adc0_settled(const char *db_path, int source_failed,
                                      int table_exists, int marker_row);

/* PoP: w20_hostedroom_b5adc0_select_expressions @ gateway/hosted_rooms_legacy_import.py:_select_expressions
 * `(target column, source expression)` pairs for copying `name`:
 *   source columns present in the target, in source order, then the
 *   _LEGACY_COLUMNS defaults for target-only columns (in defaults order).
 * `source_cols`/`target_cols` are string arrays; `defaults` is an array of
 * [table, column, default] entries (the non-None _LEGACY_COLUMNS rows). */
json_t *w20_hostedroom_b5adc0_select_expressions(const char *name,
                                                 const json_t *source_cols,
                                                 const json_t *target_cols,
                                                 const json_t *defaults);

/* PoP: w20_hostedroom_b5adc0_plan_tables @ gateway/hosted_rooms_legacy_import.py:_copy_rows
 * The tables a copy walks, in order: everything but the liveness/marker/
 * ephemeral tables, parents first (`hosted_rooms` then alphabetical). */
json_t *w20_hostedroom_b5adc0_plan_tables(const json_t *names);

/* PoP: w20_hostedroom_b5adc0_table_included @ gateway/hosted_rooms_legacy_import.py:_copy_rows
 * `name not in _SKIP_TABLES and name != MARKER_TABLE and not
 * name.endswith(("_next", "_migrating"))`. */
int w20_hostedroom_b5adc0_table_included(const char *name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_HOSTEDROOM_B5ADC0_H */
