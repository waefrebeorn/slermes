/*
 * port_state_repair.h -- pure helpers ported from hermes_state_repair.py.
 *
 * The SQLite surgery / locking / ledger half stays in Python.
 */
#ifndef PORT_STATE_REPAIR_H
#define PORT_STATE_REPAIR_H

#include <stddef.h>

/* _mask_volatile_header -- zero the commit-counter fields (24-27 and 92-95) so
 * ordinary writes do not re-key the ledger. A header shorter than 96 bytes is
 * left unchanged. Modifies `head` in place. */
void sr_mask_volatile_header(unsigned char *head, size_t len);

/* _repair_backup_headroom_bytes -- free space required beyond the copy:
 * max(256 MiB, 2% of the volume). */
long long sr_backup_headroom(long long total_bytes);

/* _repair_failure_consumes_attempt -- only SQLite's corruption/image result
 * codes prove deterministic damage; environmental failures must not burn the
 * repair ledger. `error_code` 0 means "no result-code attribute" (older
 * sqlite3), which falls back to a narrow message match. */
int sr_failure_consumes(int is_db_error, int error_code, const char *message);

/* _sidecars -- the three SQLite sidecar names for a db basename
 * ("-wal", "-shm", "-journal"). Caller frees each string and the array. */
char **sr_sidecar_names(const char *db_name, int *out_count);

#endif /* PORT_STATE_REPAIR_H */
