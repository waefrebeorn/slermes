/* hermes_state_open.c — hermes_state_db lifecycle: open, schema bootstrap,
 * close. Faithful port of SessionDB's sqlite init (sessions + messages +
 * session_model_usage + schema_version). Self-contained unit; see
 * hermes_state_internal.h for the shared handle.
 */

#include "hermes_state_internal.h"
#include <stdlib.h>
#include <sys/time.h>

const char *HERMES_STATE_SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL);"
    "CREATE TABLE IF NOT EXISTS sessions ("
    "    id TEXT PRIMARY KEY,"
    "    source TEXT NOT NULL,"
    "    user_id TEXT,"
    "    session_key TEXT,"
    "    chat_id TEXT,"
    "    chat_type TEXT,"
    "    thread_id TEXT,"
    "    display_name TEXT,"
    "    origin_json TEXT,"
    "    expiry_finalized INTEGER DEFAULT 0,"
    "    model TEXT,"
    "    model_config TEXT,"
    "    system_prompt TEXT,"
    "    parent_session_id TEXT,"
    "    started_at REAL NOT NULL,"
    "    ended_at REAL,"
    "    end_reason TEXT,"
    "    message_count INTEGER DEFAULT 0,"
    "    tool_call_count INTEGER DEFAULT 0,"
    "    input_tokens INTEGER DEFAULT 0,"
    "    output_tokens INTEGER DEFAULT 0,"
    "    cache_read_tokens INTEGER DEFAULT 0,"
    "    cache_write_tokens INTEGER DEFAULT 0,"
    "    reasoning_tokens INTEGER DEFAULT 0,"
    "    cwd TEXT,"
    "    git_branch TEXT,"
    "    git_repo_root TEXT,"
    "    billing_provider TEXT,"
    "    billing_base_url TEXT,"
    "    billing_mode TEXT,"
    "    estimated_cost_usd REAL,"
    "    actual_cost_usd REAL,"
    "    cost_status TEXT,"
    "    cost_source TEXT,"
    "    pricing_version TEXT,"
    "    title TEXT,"
    "    api_call_count INTEGER DEFAULT 0,"
    "    handoff_state TEXT,"
    "    handoff_platform TEXT,"
    "    handoff_error TEXT,"
    "    compression_failure_cooldown_until REAL,"
    "    compression_failure_error TEXT,"
    "    compression_fallback_streak INTEGER NOT NULL DEFAULT 0,"
    "    compression_ineffective_count INTEGER NOT NULL DEFAULT 0,"
    "    profile_name TEXT,"
    "    rewind_count INTEGER NOT NULL DEFAULT 0,"
    "    archived INTEGER NOT NULL DEFAULT 0,"
    "    pinned INTEGER NOT NULL DEFAULT 0,"
    "    FOREIGN KEY (parent_session_id) REFERENCES sessions(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS messages ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    session_id TEXT NOT NULL REFERENCES sessions(id),"
    "    role TEXT NOT NULL,"
    "    content TEXT,"
    "    tool_call_id TEXT,"
    "    tool_calls TEXT,"
    "    tool_name TEXT,"
    "    effect_disposition TEXT,"
    "    timestamp REAL NOT NULL,"
    "    token_count INTEGER,"
    "    finish_reason TEXT,"
    "    reasoning TEXT,"
    "    reasoning_content TEXT,"
    "    reasoning_details TEXT,"
    "    codex_reasoning_items TEXT,"
    "    codex_message_items TEXT,"
    "    platform_message_id TEXT,"
    "    observed INTEGER DEFAULT 0,"
    "    active INTEGER DEFAULT 1,"
    "    api_content TEXT,"
    "    display_kind TEXT,"
    "    display_metadata TEXT"
    ");"
    "CREATE TABLE IF NOT EXISTS session_model_usage ("
    "    session_id TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,"
    "    model TEXT NOT NULL,"
    "    billing_provider TEXT NOT NULL DEFAULT '',"
    "    billing_base_url TEXT NOT NULL DEFAULT '',"
    "    billing_mode TEXT NOT NULL DEFAULT '',"
    "    task TEXT NOT NULL DEFAULT '',"
    "    api_call_count INTEGER NOT NULL DEFAULT 0,"
    "    input_tokens INTEGER NOT NULL DEFAULT 0,"
    "    output_tokens INTEGER NOT NULL DEFAULT 0,"
    "    cache_read_tokens INTEGER NOT NULL DEFAULT 0,"
    "    cache_write_tokens INTEGER NOT NULL DEFAULT 0,"
    "    reasoning_tokens INTEGER NOT NULL DEFAULT 0,"
    "    estimated_cost_usd REAL NOT NULL DEFAULT 0,"
    "    actual_cost_usd REAL NOT NULL DEFAULT 0,"
    "    cost_status TEXT,"
    "    cost_source TEXT,"
    "    first_seen REAL,"
    "    last_seen REAL,"
    "    PRIMARY KEY (session_id, model, billing_provider, billing_base_url,"
    "                 billing_mode, task)"
    ");";

double hermes_state_now_epoch(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

/* PoP: __init__ (open + schema) @ hermes_state.py:SessionDB.__init__ */
hermes_state_db_t *hermes_state_db_open(const char *path) {
    if (!path || !*path) return NULL;
    hermes_state_db_t *h = calloc(1, sizeof(*h));
    if (!h) return NULL;
    if (sqlite3_open_v2(path, &h->db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                        SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK) {
        if (h->db) sqlite3_close(h->db);
        free(h);
        return NULL;
    }
    sqlite3_busy_timeout(h->db, 5000);
    sqlite3_exec(h->db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    char *err = NULL;
    if (sqlite3_exec(h->db, HERMES_STATE_SCHEMA_SQL, NULL, NULL, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        sqlite3_close(h->db);
        free(h);
        return NULL;
    }
    sqlite3_exec(h->db,
        "INSERT OR IGNORE INTO schema_version (version) VALUES (1);",
        NULL, NULL, NULL);
    return h;
}

void hermes_state_db_close(hermes_state_db_t *db) {
    if (!db) return;
    if (db->db) sqlite3_close(db->db);
    free(db);
}
