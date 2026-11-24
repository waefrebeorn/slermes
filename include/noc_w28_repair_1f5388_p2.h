/*
 * noc_w28_repair_1f5388_p2.h - C surface for upstream
 * persistence/sqlite/_state/repair.py, gate w28_repair_1f5388_p2.
 *
 * The classification and statement-construction layer: which integrity_check
 * lines are structural damage rather than rebuildable FTS shadow damage, the
 * probe's problem summary, the statement stream each repair strategy executes
 * (byte-exact, including the sqlite_master surgery and the VACUUM), the health
 * probe's own plan, and the ledger/backup naming + attempt arithmetic.
 *
 * Seams (not here): sqlite connections (the statements are built, not run),
 * iterdir()/unlink()/copy2(), the wall clock, sha256.
 */
#ifndef NOC_W28_REPAIR_1F5388_P2_H
#define NOC_W28_REPAIR_1F5388_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* integrity_damage_is_structural(integrity_lines, master_rows):
 * `master_rows` is [[rootpage, type, name], ...] as sqlite_master returns it. */
json_t *w28_repair_1f5388_integrity_structural(const json_t *lines, const json_t *master_rows);

/* _db_opens_cleanly's problem summary: "; ".join(problems[:3]) where a row is
 * a problem when it is non-empty and str(row[0]).lower() != "ok"; null when
 * the probe continues past integrity_check (all_ok, i.e. no problems). */
json_t *w28_repair_1f5388_probe_reason(const json_t *rows, int all_ok);

/* The statements one _REPAIR_STRATEGIES body executes, in order, as
 * [{"sql": ..., "params": [...]}, ...].  `name` is the strategy's table name;
 * `dupes` is the sqlite_master duplicate list the dedup SELECT returned
 * ([[type, name, count, keep], ...]) and `schema_version` the PRAGMA value
 * read before the cookie bump. */
json_t *w28_repair_1f5388_strategy_statements(const char *name, const json_t *dupes,
                                              double schema_version);

/* _db_opens_cleanly's statement plan on a healthy store, in order. */
json_t *w28_repair_1f5388_probe_plan(void);

/* _bump_schema_cookie(): the PRAGMA schema_version text for `current`
 * (wrapped into SQLite's signed 32-bit range). */
json_t *w28_repair_1f5388_schema_version_sql(double current);

/* _db_fingerprint's key text: f"{size}:{sha256hex[:32]}" (the hash is a seam). */
json_t *w28_repair_1f5388_fingerprint_text(double size, const char *digest_hex);

/* _repair_ledger_path(db_path).name */
json_t *w28_repair_1f5388_ledger_name(const char *db_name);

/* repair scratch/backup sibling names (db_name + fixed suffix). */
json_t *w28_repair_1f5388_scratch_name(const char *db_name);
json_t *w28_repair_1f5388_staging_name(const char *db_name, const char *stamp);

/* _backup_db_file's collision loop: the first "<name>.malformed-backup-<stamp>"
 * (then "<stamp>_<seq>" from 1) that is not in `existing`. */
json_t *w28_repair_1f5388_backup_name(const char *db_name, const char *stamp, const json_t *existing);

/* _existing_malformed_backups(): startswith the prefix, not a sidecar of one,
 * sorted by name newest-first. */
json_t *w28_repair_1f5388_existing_backups(const char *db_name, const json_t *names);

/* _prune_malformed_backups(): every path unlink()ed (stale backups and their
 * sidecars), in order; `keep` is the retention cap. */
json_t *w28_repair_1f5388_prune_victims(const char *db_name, const json_t *names, double keep);

/* _persistent_repair_attempts_exhausted(): same-key test then the budget.
 * `recorded`/`fp` are null when the ledger key is missing / the fingerprint is
 * unavailable. */
json_t *w28_repair_1f5388_attempts_exhausted(const json_t *recorded, const json_t *fp,
                                             double db_size, const json_t *failed_attempts);

/* _record_repair_outcome()'s ledger arithmetic: the new failed_attempts, or
 * null when the pass records nothing (no prior key and no mintable one). */
json_t *w28_repair_1f5388_next_attempts(const json_t *recorded, const json_t *fp,
                                        const json_t *failed_attempts);

#ifdef __cplusplus
}
#endif
#endif
