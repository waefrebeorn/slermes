/*
 * port_state_repair.c
 *
 * Pure helpers ported from hermes_state_repair.py (56 defs, previously 0
 * ported).
 *
 *   sr_mask_volatile_header   <- _mask_volatile_header
 *   sr_backup_headroom        <- _repair_backup_headroom_bytes
 *   sr_failure_consumes       <- _repair_failure_consumes_attempt
 *   sr_sidecar_names          <- _sidecars
 *
 * The SQLite surgery / locking / ledger half stays in Python.
 */

#include "port_state_repair.h"

#include <stdlib.h>
#include <string.h>

/* hermes_state_repair.py:_FINGERPRINT_VOLATILE_HEADER_RANGES
 * The file change counter (24-27) and the version-valid-for number (92-95) move
 * on ordinary commits, not on repair. */
static const int SR_VOLATILE_RANGES[][2] = { { 24, 28 }, { 92, 96 } };
#define SR_VOLATILE_N (sizeof(SR_VOLATILE_RANGES) / sizeof(SR_VOLATILE_RANGES[0]))

/* hermes_state_repair.py:_DB_SIDECAR_SUFFIXES */
static const char *const SR_SIDECAR_SUFFIXES[] = { "-wal", "-shm", "-journal" };
#define SR_SIDECAR_N (sizeof(SR_SIDECAR_SUFFIXES) / sizeof(SR_SIDECAR_SUFFIXES[0]))

/* hermes_state_repair.py:_REPAIR_BACKUP_MIN_FREE_BYTES (256 MiB) and
 * _REPAIR_BACKUP_FREE_FRACTION (2%). */
#define SR_MIN_FREE_BYTES (256LL * 1024 * 1024)
#define SR_FREE_FRACTION_NUM 2
#define SR_FREE_FRACTION_DEN 100

/* sqlite3.SQLITE_CORRUPT / SQLITE_NOTADB */
#define SR_SQLITE_CORRUPT 11
#define SR_SQLITE_NOTADB 26

/* ===========================================================================
 *  _mask_volatile_header  (hermes_state_repair.py:_mask_volatile_header)
 *
 *      if len(head) < 96: return head
 *      buf = bytearray(head)
 *      for start, end in _FINGERPRINT_VOLATILE_HEADER_RANGES:
 *          buf[start:end] = b"\x00" * (end - start)
 *      return bytes(buf)
 *
 *  Zeroes the commit-counter fields so ordinary writes do not re-key the ledger.
 *  A short header is returned UNCHANGED. `head` is modified in place.
 * =========================================================================== */

/* PoP: _mask_volatile_header @ hermes_state_repair.py:_mask_volatile_header */
void sr_mask_volatile_header(unsigned char *head, size_t len)
{
    if (!head || len < 96) return;
    for (size_t i = 0; i < SR_VOLATILE_N; i++) {
        int start = SR_VOLATILE_RANGES[i][0];
        int end = SR_VOLATILE_RANGES[i][1];
        if ((size_t)end <= len)
            memset(head + start, 0, (size_t)(end - start));
    }
}

/* ===========================================================================
 *  _repair_backup_headroom_bytes  (hermes_state_repair.py:_repair_backup_headroom_bytes)
 *
 *      return max(_REPAIR_BACKUP_MIN_FREE_BYTES,
 *                 int(total_bytes * _REPAIR_BACKUP_FREE_FRACTION))
 *
 *  Free space required BEYOND the copy itself. Proportional, not a flat
 *  multi-GB floor: a refused backup is a hard stop, and a big flat reserve would
 *  make repair never run on small volumes.
 * =========================================================================== */

/* PoP: _repair_backup_headroom_bytes @ hermes_state_repair.py:_repair_backup_headroom_bytes */
long long sr_backup_headroom(long long total_bytes)
{
    long long proportional = total_bytes / SR_FREE_FRACTION_DEN * SR_FREE_FRACTION_NUM;
    return proportional > SR_MIN_FREE_BYTES ? proportional : SR_MIN_FREE_BYTES;
}

/* ===========================================================================
 *  _repair_failure_consumes_attempt  (hermes_state_repair.py:_repair_failure_consumes_attempt)
 *
 *      if not isinstance(exc, sqlite3.DatabaseError): return False
 *      if isinstance(error_code := getattr(exc, "sqlite_errorcode", None), int):
 *          return (error_code & 0xFF) in (SQLITE_CORRUPT, SQLITE_NOTADB)
 *      return any(m in str(exc).lower() for m in ("file is not a database",
 *                                                 "database disk image is malformed"))
 *
 *  Lock contention, timeouts, disk-full and I/O failures are ENVIRONMENTAL - a
 *  retry may succeed, so they must not burn the repair ledger. Only SQLite's
 *  corruption/image result codes prove deterministic damage.
 *
 *  `is_db_error` stands in for isinstance(exc, sqlite3.DatabaseError);
 *  `error_code` is sqlite_errorcode (0 = absent); `message` is str(exc).
 * =========================================================================== */

/* PoP: _repair_failure_consumes_attempt @ hermes_state_repair.py:_repair_failure_consumes_attempt */
int sr_failure_consumes(int is_db_error, int error_code, const char *message)
{
    if (!is_db_error) return 0;

    if (error_code) {
        int primary = error_code & 0xFF;   /* extended codes keep the primary in the low byte */
        return primary == SR_SQLITE_CORRUPT || primary == SR_SQLITE_NOTADB;
    }

    /* Older sqlite3 without result-code attributes: a narrow message match only,
     * never turning a generic "disk is full"/"readonly" into a permanent failure. */
    const char *m = message ? message : "";
    char *low = (char *)malloc(strlen(m) + 1);
    if (!low) return 0;
    for (size_t i = 0; m[i]; i++)
        low[i] = (char)((m[i] >= 'A' && m[i] <= 'Z') ? m[i] + 32 : m[i]);
    low[strlen(m)] = '\0';

    int hit = (strstr(low, "file is not a database") != NULL) ||
              (strstr(low, "database disk image is malformed") != NULL);
    free(low);
    return hit;
}

/* ===========================================================================
 *  _sidecars  (hermes_state_repair.py:_sidecars)
 *
 *      return (db_path.with_name(db_path.name + suffix)
 *              for suffix in _DB_SIDECAR_SUFFIXES)
 *
 *  The three SQLite sidecar paths for a db (present or not). `db_name` is the
 *  basename; returns a malloc'd NULL-terminated array of malloc'd strings.
 *  Caller frees.
 * =========================================================================== */

/* PoP: _sidecars @ hermes_state_repair.py:_sidecars */
char **sr_sidecar_names(const char *db_name, int *out_count)
{
    char **out = (char **)calloc(SR_SIDECAR_N + 1, sizeof(char *));
    if (!out) { if (out_count) *out_count = 0; return NULL; }

    size_t base = strlen(db_name ? db_name : "");
    for (size_t i = 0; i < SR_SIDECAR_N; i++) {
        size_t sl = strlen(SR_SIDECAR_SUFFIXES[i]);
        out[i] = (char *)malloc(base + sl + 1);
        if (out[i]) {
            memcpy(out[i], db_name ? db_name : "", base);
            memcpy(out[i] + base, SR_SIDECAR_SUFFIXES[i], sl + 1);
        }
    }
    if (out_count) *out_count = (int)SR_SIDECAR_N;
    return out;
}
