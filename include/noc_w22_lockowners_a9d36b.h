#ifndef NOC_W22_LOCKOWNERS_A9D36B_H
#define NOC_W22_LOCKOWNERS_A9D36B_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/_state/lockowners.py (gate w22_lockowners_a9d36b).
 *
 * `state_db_write_lock_holders` stats state.db and its sidecars (the
 * filesystem seam) and reads /proc/locks; the pure work is the line parser and
 * the human name for a byte range. The kernel-served ident carries the device
 * as hex major:minor, so the parser rebuilds dev_t with Linux's glibc packing.
 *
 * inodes: [{"major": int, "minor": int, "ino": int|decimal-string,
 *           "sidecar": ""|"-wal"|"-shm"}]
 */

/* PoP: w22_lockowners_a9d36b_describe_range @ persistence/sqlite/_state/lockowners.py:_describe_range
 * The operator-facing name of the locked byte range. */
char *w22_lockowners_a9d36b_describe_range(const char *sidecar, long long start,
                                           long long end);

/* PoP: w22_lockowners_a9d36b_parse_proc_locks @ persistence/sqlite/_state/lockowners.py:parse_proc_locks
 * JSON array of [pid, kind, sidecar] for every WRITE lock on a known inode;
 * read locks, blocked waiters and unparseable lines are dropped. */
json_t *w22_lockowners_a9d36b_parse_proc_locks(const char *text, const json_t *inodes);

/* PoP: w22_lockowners_a9d36b_warning_message @ persistence/sqlite/_state/lockowners.py:log_write_lock_holders
 * The exact WARNING text (no trailing newline) for the deadline message;
 * `path_name` is the resolved basename the caller already computed. */
char *w22_lockowners_a9d36b_warning_message(const char *path_name, double patience_s,
                                            const json_t *holders);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_LOCKOWNERS_A9D36B_H */
