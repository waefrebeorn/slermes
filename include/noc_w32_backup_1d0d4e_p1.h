/*
 * noc_w32_backup_1d0d4e_p1.h - C surface for upstream
 * persistence/sqlite/admin/_backup.py (gate w32_backup_1d0d4e_p1).
 *
 * The emergency-snapshot layout of BackupAdmin.preflight_backup and the
 * stepped rollback-journal copy budget of _stepped_rollback_journal_copy:
 * the stamp, the three file names (plus the restore temp names), which staged
 * *.partial* files are reclaimed, which *.bak files the two-newest retention
 * keeps, and the page budget / refusal of a stepped copy.  The filesystem,
 * sqlite handles and the clock stay in the caller; every function here is a
 * pure function of the values in its request object.
 */
#ifndef NOC_W32_BACKUP_1D0D4E_P1_H
#define NOC_W32_BACKUP_1D0D4E_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"epoch": <utc seconds>} -> "%Y-%m-%dT%H-%M-%S-%fZ" (microseconds). */
json_t *w32_backup_1d0d4e_p1_emergency_stamp(const json_t *req);

/* {"home","stamp","pid","db_name"} ->
 * {destination, staged, journal, tmp_restore, sidecars:[wal,shm,journal]}. */
json_t *w32_backup_1d0d4e_p1_emergency_paths(const json_t *req);

/* {"names":[...]} -> {"ordered":[...newest first...],"prune":[...after two...]}. */
json_t *w32_backup_1d0d4e_p1_prune_survivors(const json_t *req);

/* {"names":[...],"mtimes":[...],"now"} -> {"removed":[...],"survivors":[...]}. */
json_t *w32_backup_1d0d4e_p1_reclaim_staging(const json_t *req);

/* {"rate_pages","rate_seconds"} -> {"pages_per_second","next_pages"}. */
json_t *w32_backup_1d0d4e_p1_step_plan(const json_t *req);

/* {"rate_pages","rate_seconds","total"} -> {"expected_hold","refuse","message"}. */
json_t *w32_backup_1d0d4e_p1_hold_budget(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
