/*
 * hermes_state_schema.c — organization twin of hermes_state_schema.py.
 *
 * Schema creation, column reconciliation, and FTS DDL management for the
 * SessionDB. The schema DDL itself (SCHEMA_SQL / FTS_SQL / DEFERRED_INDEX_SQL)
 * is ported in hermes_state_open.c as HERMES_STATE_SCHEMA_SQL (extend-
 * don't-duplicate); this module provides the schema-management entry points
 * that the Python mixin exposes, delegating to the shared open/ports layer.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

/* PoP: hs_schema_version @ hermes_state_schema.py:SCHEMA_VERSION (via hermes_state_common) */
int hs_schema_version(void) {
    extern int hs_common_schema_version(void);
    return hs_common_schema_version();
}

/* PoP: hs_schema_fts_storage_version @ hermes_state_schema.py:FTS_STORAGE_VERSION */
int hs_schema_fts_storage_version(void) {
    extern int hs_common_fts_storage_version(void);
    return hs_common_fts_storage_version();
}

/* PoP: hs_schema_ddl @ hermes_state_schema.py:SCHEMA_SQL */
const char *hs_schema_ddl(void) {
    extern const char *hs_common_schema_sql(void);
    return hs_common_schema_sql();
}

/* PoP: hs_schema_apply @ hermes_state_schema.py:_ensure_schema
 * Applies the canonical DDL to an open database. Idempotent: every statement
 * is CREATE TABLE/INDEX/TRIGGER IF NOT EXISTS.
 *
 * IMPORTANT: do NOT split this DDL on ';' — it contains CREATE TRIGGER bodies
 * whose internal statements end with ';' (messages_fts_insert etc.). The
 * Python side uses executescript(), which handles trigger bodies correctly;
 * sqlite3_exec() does too, because it also executes multiple statements per
 * call. We delegate to sqlite3_exec on the whole buffer, matching
 * hermes_state_db_open(). Returns 0 on success. */
int hs_schema_apply(sqlite3 *db, char *err, size_t err_sz) {
    extern const char *HERMES_STATE_SCHEMA_SQL;
    const char *ddl = HERMES_STATE_SCHEMA_SQL;
    char *errmsg = NULL;
    if (sqlite3_exec(db, ddl, NULL, NULL, &errmsg) != SQLITE_OK) {
        if (err && err_sz)
            snprintf(err, err_sz, "%s", errmsg ? errmsg : "sqlite error");
        sqlite3_free(errmsg);
        return 2;
    }
    sqlite3_free(errmsg);
    return 0;
}

/* PoP: hs_schema_set_meta @ hermes_state_schema.py state_meta key/value helper */
int hs_schema_set_meta(sqlite3 *db, const char *key, const char *value,
                       char *err, size_t err_sz) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO state_meta (key, value) VALUES (?1, ?2) "
        "ON CONFLICT(key) DO UPDATE SET value = ?2",
        -1, &st, NULL) != SQLITE_OK) {
        if (err && err_sz) snprintf(err, err_sz, "%s", sqlite3_errmsg(db));
        return 1;
    }
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, value, -1, SQLITE_STATIC);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) {
        if (err && err_sz) snprintf(err, err_sz, "%s", sqlite3_errmsg(db));
        return 2;
    }
    return 0;
}
