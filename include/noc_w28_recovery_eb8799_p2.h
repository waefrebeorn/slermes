/*
 * noc_w28_recovery_eb8799_p2.h - C surface for upstream
 * persistence/sqlite/admin/_recovery.py (gate w28_recovery_eb8799_p2).
 *
 * Salvage geometry and verification classification of the offline recovery
 * lane.  A source table is a fixture rowid set plus damaged candidate
 * intervals; integrity/FK/count observations are fixture values.  This surface
 * decides the bounds, the repair SQL text, the loss classification and the
 * final verdict -- never opens SQLite.
 */
#ifndef NOC_W28_RECOVERY_EB8799_P2_H
#define NOC_W28_RECOVERY_EB8799_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Galloping finite bound for a damaged rowid edge:
 * {"edge", "probes", "capped", "bound"} */
json_t *w28_rec_p2_probe_populated_edge(const json_t *occupied, const json_t *damage,
                                        const char *edge, double anchor);

/* req = {occupied, damage, error, low_ok?, high_ok?, aggregate_ok?} ->
 * {"errors", "fallback_edges", ("empty"|"unavailable")?, "aggregate_edges"?,
 *  "low", "high", "edge_probes"?}. */
json_t *w28_rec_p2_salvage_rowid_bounds(const json_t *req);

/* req = {messages_columns, sessions_columns, repair_count} ->
 * {"repaired", "statements"}; every out-of-window UPDATE is emitted byte-for-byte. */
json_t *w28_rec_p2_repair_plan(const json_t *req);

/* req = {integrity_rows, foreign_key_rows, journal_mode, schema_version|null,
 *        fts_meta} -> the verification object's errors/flags. */
json_t *w28_rec_p2_verify_structure(const json_t *req);

/* req = {present_tables, counts, expected_counts, copy_report, allow_partial,
 *        orphan_cleanup|null} -> {"errors","warnings","loss_detected","table_counts"}. */
json_t *w28_rec_p2_verify_row_counts(const json_t *req);

/* req = {sessions:{total,implausible}, messages:{total,implausible}} -> [error, ...]. */
json_t *w28_rec_p2_lost_and_found_plausibility_errors(const json_t *req);

/* req = {source, output, inspection, disk_space, verification, source_unchanged,
 *        on_source_change, fields?} -> the recover report with its verdict. */
json_t *w28_rec_p2_recovery_report(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
