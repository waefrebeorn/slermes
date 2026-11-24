/*
 * noc_w20_hostedroom_d4f060.h -- C11 port of the pre-isolation row import of
 * upstream persistence/sqlite/gateway/_hosted_rooms_legacy_import.py.
 */
#ifndef NOC_W20_HOSTEDROOM_D4F060_H
#define NOC_W20_HOSTEDROOM_D4F060_H

#include "hermes_json.h"

/* source_path(db_path): "<dir>/state.db" for a shared-state.db, else NULL. Caller frees. */
char *w20_hostedroom_d4f060_source_path(const char *db_path);

/* "hosted_room_legacy_imports" */
const char *w20_hostedroom_d4f060_marker_table(void);

/*
 * settled(), with its two I/O seams as data: whether the source is already in
 * the process's _failed_sources set, whether the marker table exists, and
 * whether the marker row for SOURCE_NAME is present.
 */
int w20_hostedroom_d4f060_settled(const char *db_path, int in_failed_sources,
                                  int marker_table_exists, int marker_row_present);

/*
 * _select_expressions() for table ``name``: (target column, source expression)
 * pairs, source order first then the _LEGACY_COLUMNS defaults the target has
 * but the source does not.  ``legacy_columns`` rows are [table, column, type,
 * default] as _hosted_rooms_schema._LEGACY_COLUMNS carries them.
 */
json_t *w20_hostedroom_d4f060_select_expressions(const char *name,
                                                 const json_t *source_columns,
                                                 const json_t *target_columns,
                                                 const json_t *legacy_columns);

/* The _copy_rows table filter: _SKIP_TABLES, the marker table, _next/_migrating. */
int w20_hostedroom_d4f060_table_skipped(const char *name);

/* sorted(names, key=lambda n: (n != "hosted_rooms", n)) -- parents first. */
json_t *w20_hostedroom_d4f060_ordered_tables(const json_t *names);

#endif /* NOC_W20_HOSTEDROOM_D4F060_H */
