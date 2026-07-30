/* Slermes C11 port of hermes_cli/sqlite_util.py
 *
 * Shared SQLite primitives for the small per-profile / board stores:
 *  - add_column_if_missing: idempotent ALTER TABLE ADD COLUMN (swallows the
 *    "duplicate column name" race, re-raises other OperationalErrors).
 *  - write_txn: IMMEDIATE write transaction with a guarded ROLLBACK so a
 *    SQLite auto-rollback cannot shadow the original exception.
 *
 * Uses the bundled SQLite (lib/libdb/sqlite3.c). Pure C11.
 *
 * PoP: exact port. Semantic source of truth = hermes_cli/sqlite_util.py.
 */
#ifndef SLERMES_SQLITE_UTIL_H
#define SLERMES_SQLITE_UTIL_H

#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error/result codes for add_column_if_missing. */
typedef enum {
    SQLITE_UTIL_ADD_ADDED   =  1,  /* this call added the column */
    SQLITE_UTIL_ADD_EXISTS  =  0,  /* duplicate column name: already present */
    SQLITE_UTIL_ADD_ERROR   = -1   /* other OperationalError (caller should treat as failure) */
} sqlite_util_add_result_t;

/* Idempotent ALTER TABLE <table> ADD COLUMN <ddl>. Returns ADDED / EXISTS /
 * ERROR. Matches Python: swallows "duplicate column name", surfaces others
 * (here as ERROR rather than raising, since C has no exceptions). */
sqlite_util_add_result_t sqlite_util_add_column_if_missing(
    sqlite3 *conn, const char *table, const char *column, const char *ddl);

/* Write-transaction primitives (mirror the Python context manager).
 * begin returns 0 on success, non-zero on error. end commits on success or
 * performs a guarded ROLLBACK (swallowing a no-active-txn error). */
int sqlite_util_write_txn_begin(sqlite3 *conn);
int sqlite_util_write_txn_end(sqlite3 *conn, int committed);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_SQLITE_UTIL_H */
