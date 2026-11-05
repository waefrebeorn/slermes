#ifndef PORT_BACKUP_SQLITE_H
#define PORT_BACKUP_SQLITE_H

#include "hermes_json.h"

/* Port of Python: hermes_cli/backup_sqlite.py
 *
 * WAL-safe SQLite snapshots for the state DB: a consistent copy via the
 * sqlite3 backup API, an owner-only staging file, a quick_check gate, an
 * abandoned-staging sweep and a keep-two prune.
 *
 * Contract preserved: fail closed. If no consistent snapshot can be taken the
 * function leaves the destination absent and reports the failure; it never
 * copies just the main file of a WAL database.
 */

/* Header byte 18 == 2 means the file is in WAL mode (no lock is taken). */
int bsq_uses_wal(const char *path);

/* Remove `<home>/<prefix>*.partial*` older than the abandoned horizon. */
void bsq_reclaim_abandoned_staging(const char *home, const char *prefix);

/* Publish an emergency snapshot of <home>/state.db.
 * Returns a JSON object: {"path": <string>, "bytes": <number>} on success or
 * {"path": null, "message": <string>} when there is nothing to take. */
json_t *bsq_preflight_state_db(const char *home);

/* One-shot: the small-file copy used by callers that already hold a
 * destination path. Returns 1 on success, 0 on failure (destination removed). */
int bsq_safe_copy_db(const char *src, const char *dst, double timeout_seconds);

#endif /* PORT_BACKUP_SQLITE_H */
