/*
 * test_state_db.c — State Database Tests
 * Tests SQLite state database operations: create, read, write, schema.
 *
 * Build: gcc -O2 -g -I include -o test_state_db tests/state_db/test_state_db.c \
 *        lib/libdb/sqlite3.o lib/libdb/db.o -lm -lpthread
 *
 * Run: ./test_state_db
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "sqlite3.h"

#define TEST_PASS 0
#define TEST_FAIL 1

static int g_pass = 0;
static int g_fail = 0;
static sqlite3 *g_db = NULL;

static void test_assert(const char *name, int condition) {
    if (condition) {
        printf("  PASS: %s\n", name);
        g_pass++;
    } else {
        printf("  FAIL: %s\n", name);
        g_fail++;
    }
}

static void db_log(void *ud, int errcode, const char *msg) {
    (void)ud; (void)errcode;
    if (msg) fprintf(stderr, "[db] %s\n", msg);
}

int main(void) {
    printf("=== State DB Tests ===\n\n");

    /* Open in-memory database */
    printf("--- Open ---\n");
    int rc = sqlite3_open(":memory:", &g_db);
    test_assert("sqlite3_open succeeds", rc == SQLITE_OK);

    /* Create sessions table */
    printf("\n--- Create Table ---\n");
    const char *create_sql =
        "CREATE TABLE IF NOT EXISTS sessions ("
        "  id TEXT PRIMARY KEY,"
        "  model TEXT NOT NULL,"
        "  platform TEXT DEFAULT 'local',"
        "  chat_type TEXT DEFAULT 'dm',"
        "  created_at INTEGER,"
        "  updated_at INTEGER,"
        "  input_tokens INTEGER DEFAULT 0,"
        "  output_tokens INTEGER DEFAULT 0"
        ");";
    char *err = NULL;
    rc = sqlite3_exec(g_db, create_sql, NULL, NULL, &err);
    test_assert("CREATE TABLE succeeds", rc == SQLITE_OK);
    if (err) sqlite3_free(err);

    /* Create messages table */
    const char *create_msgs =
        "CREATE TABLE IF NOT EXISTS messages ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  session_id TEXT NOT NULL,"
        "  role TEXT NOT NULL,"
        "  content TEXT,"
        "  timestamp INTEGER,"
        "  FOREIGN KEY(session_id) REFERENCES sessions(id)"
        ");";
    rc = sqlite3_exec(g_db, create_msgs, NULL, NULL, &err);
    test_assert("CREATE messages table succeeds", rc == SQLITE_OK);
    if (err) sqlite3_free(err);

    /* Insert a session */
    printf("\n--- Insert ---\n");
    const char *insert_sql =
        "INSERT INTO sessions VALUES ('test-001', 'claude-sonnet-4', 'telegram', 'dm', 1719000000, 1719000001, 0, 0);";
    rc = sqlite3_exec(g_db, insert_sql, NULL, NULL, &err);
    test_assert("INSERT session succeeds", rc == SQLITE_OK);
    if (err) sqlite3_free(err);

    /* Insert multiple sessions (separate exec calls) */
    const char *insert_2 =
        "INSERT INTO sessions VALUES ('test-002', 'gpt-4o', 'discord', 'group', 1719000100, 1719000101, 0, 0);";
    rc = sqlite3_exec(g_db, insert_2, NULL, NULL, &err);
    test_assert("INSERT session 2 succeeds", rc == SQLITE_OK);
    if (err) { fprintf(stderr, "  ERR: %s\n", err); sqlite3_free(err); }
    const char *insert_3 =
        "INSERT INTO sessions VALUES ('test-003', 'claude-sonnet-4', 'local', 'dm', 1719000200, 1719000201, 0, 0);";
    rc = sqlite3_exec(g_db, insert_3, NULL, NULL, &err);
    test_assert("INSERT session 3 succeeds", rc == SQLITE_OK);
    if (err) { fprintf(stderr, "  ERR: %s\n", err); sqlite3_free(err); }

    /* Count sessions */
    printf("\n--- Query ---\n");
    {
        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM sessions", -1, &stmt, NULL);
        test_assert("COUNT prepare succeeds", rc == SQLITE_OK);
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            test_assert("COUNT step succeeds", rc == SQLITE_ROW);
            int count = sqlite3_column_int(stmt, 0);
            test_assert("3 sessions exist", count == 3);
            sqlite3_finalize(stmt);
        }
    }

    /* Select by platform */
    {
        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(g_db,
            "SELECT id FROM sessions WHERE platform = 'telegram'", -1, &stmt, NULL);
        test_assert("SELECT by platform prepare succeeds", rc == SQLITE_OK);
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            test_assert("Found telegram session", rc == SQLITE_ROW);
            const char *id = (const char *)sqlite3_column_text(stmt, 0);
            test_assert("Correct session id", id && strcmp(id, "test-001") == 0);
            sqlite3_finalize(stmt);
        }
    }

    /* Insert messages */
    printf("\n--- Messages ---\n");
    const char *insert_msg =
        "INSERT INTO messages (session_id, role, content, timestamp) "
        "VALUES ('test-001', 'user', 'Hello world', 1719000000);"
        "INSERT INTO messages (session_id, role, content, timestamp) "
        "VALUES ('test-001', 'assistant', 'Hi there!', 1719000001);";
    rc = sqlite3_exec(g_db, insert_msg, NULL, NULL, &err);
    test_assert("INSERT messages succeeds", rc == SQLITE_OK);
    if (err) sqlite3_free(err);

    /* Count messages for session */
    {
        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(g_db,
            "SELECT COUNT(*) FROM messages WHERE session_id = 'test-001'",
            -1, &stmt, NULL);
        test_assert("Message count prepare succeeds", rc == SQLITE_OK);
        if (rc == SQLITE_OK) {
            sqlite3_step(stmt);
            int count = sqlite3_column_int(stmt, 0);
            test_assert("2 messages for session", count == 2);
            sqlite3_finalize(stmt);
        }
    }

    /* Update session */
    printf("\n--- Update ---\n");
    const char *update_sql =
        "UPDATE sessions SET input_tokens = 150, output_tokens = 300 "
        "WHERE id = 'test-001';";
    rc = sqlite3_exec(g_db, update_sql, NULL, NULL, &err);
    test_assert("UPDATE session succeeds", rc == SQLITE_OK);
    if (err) sqlite3_free(err);

    /* Verify update */
    {
        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(g_db,
            "SELECT input_tokens FROM sessions WHERE id = 'test-001'",
            -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_step(stmt);
            int tokens = sqlite3_column_int(stmt, 0);
            test_assert("Token count updated", tokens == 150);
            sqlite3_finalize(stmt);
        }
    }

    /* Delete session (cascade not enforced, test FK awareness) */
    printf("\n--- Delete ---\n");
    const char *delete_sql = "DELETE FROM sessions WHERE id = 'test-003';";
    rc = sqlite3_exec(g_db, delete_sql, NULL, NULL, &err);
    test_assert("DELETE session succeeds", rc == SQLITE_OK);
    if (err) sqlite3_free(err);

    /* Verify count decreased */
    {
        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM sessions", -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_step(stmt);
            int count = sqlite3_column_int(stmt, 0);
            test_assert("2 sessions after delete", count == 2);
            sqlite3_finalize(stmt);
        }
    }

    /* Schema version check */
    printf("\n--- Schema Version ---\n");
    {
        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(g_db,
            "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name",
            -1, &stmt, NULL);
        test_assert("Schema query succeeds", rc == SQLITE_OK);
        if (rc == SQLITE_OK) {
            int table_count = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) table_count++;
            test_assert("Has sessions table", table_count >= 2);
            sqlite3_finalize(stmt);
        }
    }

    /* Transaction test */
    printf("\n--- Transactions ---\n");
    rc = sqlite3_exec(g_db, "BEGIN TRANSACTION;", NULL, NULL, &err);
    test_assert("BEGIN succeeds", rc == SQLITE_OK);
    if (err) sqlite3_free(err);

    const char *insert_txn =
        "INSERT INTO sessions VALUES ('txn-001', 'test', 'local', 'dm', 1719009000, 1719009001, 0, 0);";
    rc = sqlite3_exec(g_db, insert_txn, NULL, NULL, &err);
    test_assert("INSERT in transaction succeeds", rc == SQLITE_OK);
    if (err) sqlite3_free(err);

    rc = sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, &err);
    test_assert("ROLLBACK succeeds", rc == SQLITE_OK);
    if (err) sqlite3_free(err);

    /* Verify NOT committed (rolled back) */
    {
        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(g_db,
            "SELECT COUNT(*) FROM sessions WHERE id = 'txn-001'", -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_step(stmt);
            int count = sqlite3_column_int(stmt, 0);
            test_assert("Transaction rolled back (not visible)", count == 0);
            sqlite3_finalize(stmt);
        }
    }

    /* Now test COMMIT */
    rc = sqlite3_exec(g_db, "BEGIN TRANSACTION;", NULL, NULL, &err);
    if (err) sqlite3_free(err);
    rc = sqlite3_exec(g_db, insert_txn, NULL, NULL, &err);
    if (err) sqlite3_free(err);
    rc = sqlite3_exec(g_db, "COMMIT;", NULL, NULL, &err);
    test_assert("COMMIT succeeds", rc == SQLITE_OK);
    if (err) sqlite3_free(err);

    /* Verify committed */
    {
        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(g_db,
            "SELECT COUNT(*) FROM sessions WHERE id = 'txn-001'", -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_step(stmt);
            int count = sqlite3_column_int(stmt, 0);
            test_assert("Transaction committed", count == 1);
            sqlite3_finalize(stmt);
        }
    }

    sqlite3_close(g_db);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
