/* Slermes C11 port of hermes_cli/sqlite_util.py — implementation.
 * PoP: exact port. Semantic source of truth = hermes_cli/sqlite_util.py. */
#include "sqlite_util.h"

#include <string.h>
#include <stdlib.h>

/* PoP: add_column_if_missing @ hermes_cli/sqlite_util.py:add_column_if_missing */
sqlite_util_add_result_t sqlite_util_add_column_if_missing(
    sqlite3 *conn, const char *table, const char *column, const char *ddl) {
    if (!conn || !table || !ddl) return SQLITE_UTIL_ADD_ERROR;
    char *sql = sqlite3_mprintf("ALTER TABLE %s ADD COLUMN %s", table, ddl);
    if (!sql) return SQLITE_UTIL_ADD_ERROR;
    char *err = NULL;
    int rc = sqlite3_exec(conn, sql, NULL, NULL, &err);
    sqlite3_free(sql);
    if (rc == SQLITE_OK) {
        if (err) sqlite3_free(err);
        return SQLITE_UTIL_ADD_ADDED;
    }
    /* Detect the duplicate-column race and treat it as "already exists". */
    int is_dup = 0;
    if (err) {
        const char *e = err;
        /* match "duplicate column name" case-insensitively */
        for (; *e; e++) {
            if ((e[0] == 'd' || e[0] == 'D') &&
                (e[1] == 'u' || e[1] == 'U') &&
                (e[2] == 'p' || e[2] == 'P') &&
                (e[3] == 'l' || e[3] == 'L')) {
                is_dup = 1; break;
            }
        }
    }
    if (err) sqlite3_free(err);
    if (is_dup) return SQLITE_UTIL_ADD_EXISTS;
    return SQLITE_UTIL_ADD_ERROR;
}

/* PoP: sqlite_util_write_txn_begin @ hermes_cli/sqlite_util.py:write_txn */
int sqlite_util_write_txn_begin(sqlite3 *conn) {
    if (!conn) return SQLITE_UTIL_ADD_ERROR;  /* reuse -1 as generic error */
    char *err = NULL;
    int rc = sqlite3_exec(conn, "BEGIN IMMEDIATE", NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : rc;
}

/* PoP: sqlite_util_write_txn_end @ hermes_cli/sqlite_util.py:write_txn */
int sqlite_util_write_txn_end(sqlite3 *conn, int committed) {
    if (!conn) return SQLITE_UTIL_ADD_ERROR;
    char *err = NULL;
    int rc = sqlite3_exec(conn, committed ? "COMMIT" : "ROLLBACK", NULL, NULL, &err);
    if (err) {
        /* Swallow the "no active transaction" auto-rollback case, mirroring
         * the Python context manager's guarded ROLLBACK. */
        const char *e = err;
        int no_txn = 0;
        for (; *e; e++) {
            if ((e[0] == 'n' || e[0] == 'N') &&
                (e[1] == 'o' || e[1] == 'O') &&
                (e[2] == ' ')) { no_txn = 1; break; }
        }
        sqlite3_free(err);
        if (no_txn) return 0;
        return rc == SQLITE_OK ? 0 : rc;
    }
    return rc == SQLITE_OK ? 0 : rc;
}
