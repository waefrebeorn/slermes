/* session_recovery.h — offline, non-destructive recovery for a damaged
 * Hermes session database. Faithful C11 port of
 * hermes_cli/session_recovery.py. All report structures are json_t objects
 * mirroring the Python dict shapes exactly.
 *
 * The recovery path deliberately avoids in-place repair:
 *   - the supplied source database is never opened by SQLite;
 *   - the source file and any WAL/SHM/rollback-journal sidecars are copied
 *     into a disposable working directory first;
 *   - canonical rows are copied into a newly initialized current-schema DB;
 *   - derived FTS tables and migration bookkeeping are rebuilt, not copied;
 *   - the recovered database is never installed over the active database.
 */
#ifndef SESSION_RECOVERY_H
#define SESSION_RECOVERY_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h" /* json_t */

/* Error classes (mirrors SessionRecoveryError hierarchy). */
typedef enum {
    SESSION_RECOVERY_OK = 0,
    SESSION_RECOVERY_ERROR = 1,        /* base error */
    SESSION_RECOVERY_SAFETY_ERROR = 2, /* path / overwrite / space guard */
    SESSION_RECOVERY_SOURCE_ERROR = 3  /* canonical tables unreadable */
} session_recovery_status_t;

/* Progress callback: receives a BORROWED json_t object (do not free). */
typedef void (*session_recovery_progress_cb)(const json_t *event, void *ud);

/* PoP targets — every public entry mirrors a Python def. Returns a NEW
 * json_t report on success (caller frees with json_free) or NULL with
 * *status/*errbuf set. errbuf may be NULL. */

/* inspect_session_database(source, work_dir=None) */
json_t *session_recovery_inspect(const char *source_path,
                                 const char *work_dir,
                                 session_recovery_status_t *status,
                                 char *errbuf, size_t errbuf_len);

/* recover_session_database(source, output, work_dir=None, chunk_size=1000,
 *                          progress_cb=None, allow_partial=False) */
json_t *session_recovery_recover(const char *source_path,
                                 const char *output_path,
                                 const char *work_dir,
                                 int chunk_size,
                                 session_recovery_progress_cb progress_cb,
                                 void *progress_ud,
                                 bool allow_partial,
                                 session_recovery_status_t *status,
                                 char *errbuf, size_t errbuf_len);

/* write_recovery_report(path, report) — refuses to overwrite; returns
 * malloc'd resolved destination path or NULL. */
char *session_recovery_write_report(const char *path, const json_t *report,
                                    char *errbuf, size_t errbuf_len);

/* _format_bytes(value) — "1.5 KiB" style; returns malloc'd string. */
char *session_recovery_format_bytes(long long value);

#endif /* SESSION_RECOVERY_H */
