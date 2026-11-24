/*
 * noc_w26_hostedroom_598018.h - C surface for upstream
 * persistence/sqlite/gateway/_hosted_rooms_schema.py (gate w26_hostedroom_598018).
 *
 * The room schema step as a value layer: the DDL text and the two regexes that
 * derive (table, required columns) from it, the two `.format(where=...)` SQL
 * templates the migration runs, the legacy column table, and the remote-run
 * primary-key check.  Every sqlite connection is a seam; the PRAGMA rows it
 * would return arrive as values.
 */
#ifndef NOC_W26_HOSTEDROOM_598018_H
#define NOC_W26_HOSTEDROOM_598018_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One DDL statement -> {"table": str, "columns": [..]} in source order. */
json_t *w26_hostedroom_598018_parse_ddl(const char *ddl);

/* The eight (table, required columns) pairs derived from the module DDL. */
json_t *w26_hostedroom_598018_required_columns(void);

/* _RETIRE_FROM_ROOMS.format(where=where) */
json_t *w26_hostedroom_598018_retire_sql(const char *where);

/* _EVENT_BYTES_BACKFILL.format(where=where) */
json_t *w26_hostedroom_598018_event_bytes_backfill(const char *where);

/* _LEGACY_COLUMNS as [{"table","column","declaration","default"}]. */
json_t *w26_hostedroom_598018_legacy_columns(void);

/* columns + PRAGMA table_info rows -> whether the receipt key is complete. */
json_t *w26_hostedroom_598018_remote_run_current(const json_t *columns,
                                                const json_t *pk_rows);

#ifdef __cplusplus
}
#endif
#endif
