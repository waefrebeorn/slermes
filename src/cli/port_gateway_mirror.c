/*
 * port_gateway_mirror.c — C port of gateway/mirror.py (select helpers)
 */

#include "hermes_logger.h"
#include "slermes_home.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

/* PoP: cli_gateway_mirror__append_to_sqlite @ gateway/mirror.py:_append_to_sqlite */
/* Append a message to the SQLite session database. Faithful to the Python:
 * open the state.db (best-effort), INSERT (session_id, role, content), and
 * swallow any failure as a debug log (mirror writes are best-effort and must
 * never break the message path). Returns 0 on success, -1 on any failure. */
int cli_gateway_mirror__append_to_sqlite(const char *session_id,
                                         const char *role, const char *content)
{
    if (!session_id) return -1;
    if (!role) role = "assistant";
    if (!content) content = "";

    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", slermes_home(), "state.db");

    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(path, &db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        hermes_log(LOG_DEBUG, "mirror", "SQLite open failed: %s", sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return -1;
    }

    char *err = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO messages (session_id, role, content) VALUES (%Q, %Q, %Q)",
        session_id, role, content);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        hermes_log(LOG_DEBUG, "mirror", "Mirror SQLite write failed: %s", err ? err : "unknown");
        if (err) sqlite3_free(err);
        sqlite3_close(db);
        return -1;
    }
    sqlite3_close(db);
    return 0;
}
