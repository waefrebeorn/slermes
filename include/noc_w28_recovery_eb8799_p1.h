/*
 * noc_w28_recovery_eb8799_p1.h - C surface for upstream
 * persistence/sqlite/admin/_recovery.py (gate w28_recovery_eb8799_p1).
 *
 * Path derivation, statement construction and inventory classification of the
 * offline session-database recovery lane.  Every sqlite3 handle, PRAGMA result
 * and filesystem observation is a caller-supplied value: this surface decides
 * what those values mean and which SQL text they produce, nothing else.
 */
#ifndef NOC_W28_RECOVERY_EB8799_P1_H
#define NOC_W28_RECOVERY_EB8799_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* db_path if suffix is empty, else the same file name with suffix appended. */
json_t *w28_rec_p1_sidecar_path(const char *path, const char *suffix);

/* "{amount:.1f} {unit}" over B/KiB/MiB/GiB/TiB, stopping at the first < 1024. */
json_t *w28_rec_p1_format_bytes(double value);

/* req = {bundle_bytes, output_parent, work_free_bytes, output_free_bytes, same_filesystem,
 *        work_dir, output_dir} -> the preflight report, or {"error": <refusal message>}. */
json_t *w28_rec_p1_disk_space_preflight(const json_t *req);

/* ["\"a\", \"b\"", "?, ?"] for a column-name array. */
json_t *w28_rec_p1_quoted_columns(const json_t *columns);

/* {"columns": [...], "status"?, "error"?} -- destination columns also present in source. */
json_t *w28_rec_p1_compatible_columns(const char *table, const json_t *source_columns,
                                      const json_t *destination_columns);

/* req = {table, columns|null, rows|null, columns_error?, rows_error?} ->
 * {"available", "columns", "rows", "error"?}; null columns/rows mean the PRAGMA/count raised. */
json_t *w28_rec_p1_table_inventory(const json_t *req);

/* req = {journal_mode|null, journal_error?, tables:{name:inventory}} ->
 * {"tables", "errors", "warnings", "journal_mode", "recoverable"}. */
json_t *w28_rec_p1_inspect_connection(const json_t *req);

/* Adjacent same-error ranges merge instead of appending one entry per row. */
json_t *w28_rec_p1_append_skipped_range(const json_t *ranges, double low, double high,
                                        const char *error);

/* req = {source_columns, destination_columns, source_rows|null, salvage} ->
 * terminal state_meta result object, or null when the key/value schema is usable. */
json_t *w28_rec_p1_state_meta_precheck(const json_t *req);

/* {"count": n, "statements": [count sql, mutation sql?]} -- mutation only when count != 0. */
json_t *w28_rec_p1_reconcile(const char *table, const char *where, const char *mutation,
                             double count);

#ifdef __cplusplus
}
#endif
#endif
