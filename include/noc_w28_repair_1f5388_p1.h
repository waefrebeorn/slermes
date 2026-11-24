/*
 * noc_w28_repair_1f5388_p1.h - C surface for upstream
 * persistence/sqlite/_state/repair.py, gate w28_repair_1f5388_p1.
 *
 * The repair module's budget layer: the pinned constants every guard is sized
 * with, the volatile header mask that keeps a live writer from re-keying the
 * attempt ledger, the free-space headroom arithmetic, the three disk-guard
 * verdicts (with their exact operator text), the snapshot timeout, and the
 * two failure classifiers (deterministic corruption vs environmental).
 *
 * Seams (not here): sqlite connections, stat()/disk_usage(), flock, the ledger
 * file, hashing, the clock, logging.  Sizes and exception facts arrive as
 * values.
 */
#ifndef NOC_W28_REPAIR_1F5388_P1_H
#define NOC_W28_REPAIR_1F5388_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module constants (_REPAIR_LOCK_TIMEOUT_SECONDS ... _TEMPORARY_FILE_FAILURE_CODES). */
json_t *w28_repair_1f5388_constants(void);

/* _sidecars(db_path): the three SQLite sidecar paths, in _DB_SIDECAR_SUFFIXES order. */
json_t *w28_repair_1f5388_sidecars(const char *db_path);

/* _mask_volatile_header(head): lowercase hex in, lowercase hex out.  Heads
 * shorter than 96 bytes are returned untouched. */
json_t *w28_repair_1f5388_mask_header(const char *head_hex);

/* _repair_backup_headroom_bytes(total_bytes) = max(256MiB, int(total*0.02)). */
json_t *w28_repair_1f5388_headroom(double total_bytes);

/* _repair_snapshot_timeout_seconds(source_bytes[]) =
 * max(120.0, sum(sizes) / 10MiB). */
json_t *w28_repair_1f5388_snapshot_timeout(const json_t *sizes);

/* _disk_budget()'s failure text: stat()/disk_usage() raised `exc`. */
json_t *w28_repair_1f5388_disk_budget_error(const char *parent, const char *exc, const char *refusal);

/* _repair_scratch_space_error(): null when the snapshot+VACUUM+headroom fit,
 * else the exact refusal text. */
json_t *w28_repair_1f5388_scratch_space_error(const char *parent, double snapshot_bytes,
                                              double free_bytes, double headroom_bytes);

/* _backup_free_space_error(): null when free-need>=headroom, else the refusal
 * text (which carries _MANUAL_RECOVER_HINT). */
json_t *w28_repair_1f5388_backup_free_space_error(const char *parent, double need_bytes,
                                                  double free_bytes, double headroom_bytes,
                                                  const char *db_path);

/* _MANUAL_RECOVER_HINT.format(db_path=...). */
json_t *w28_repair_1f5388_manual_hint(const char *db_path);

/* _repair_failure_consumes_attempt(exc): `is_db_error` is isinstance(exc,
 * sqlite3.DatabaseError); `errorcode_present` is getattr(...) is not None and
 * `errorcode` is its int value when it is an int (else the message path). */
json_t *w28_repair_1f5388_failure_consumes_attempt(int is_db_error, int errorcode_present,
                                                   double errorcode, const char *message);

/* _is_temporary_file_failure(failure): primary result code in
 * {3,8,10,13,14} (SQLITE_PERM/READONLY/IOERR/FULL/CANTOPEN) or a disk-full
 * message. */
json_t *w28_repair_1f5388_temporary_file_failure(int errorcode_present, double errorcode,
                                                 const char *message);

/* _schema_not_built(exc): "no such table" / "no such column" (case-folded). */
json_t *w28_repair_1f5388_schema_not_built(const char *message);

#ifdef __cplusplus
}
#endif
#endif
