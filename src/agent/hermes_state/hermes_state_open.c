/* hermes_state_open.c — hermes_state_db lifecycle: open, FULL schema
 * bootstrap (faithful to hermes_state.py SCHEMA_SQL + DEFERRED_INDEX_SQL +
 * FTS_SQL + FTS_TRIGRAM_SQL, schema v23, fts_storage_version 1), close.
 * Self-contained unit; see hermes_state_internal.h for the shared handle.
 */

#include "hermes_state_internal.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/* PoP: HERMES_STATE_SCHEMA_SQL @ hermes_state.py:SCHEMA_SQL */
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
    "    active INTEGER NOT NULL DEFAULT 1,"
    "    compacted INTEGER NOT NULL DEFAULT 0,"
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
    ");"
    "CREATE TABLE IF NOT EXISTS state_meta ("
    "    key TEXT PRIMARY KEY,"
    "    value TEXT"
    ");"
    "CREATE TABLE IF NOT EXISTS gateway_routing ("
    "    scope TEXT NOT NULL DEFAULT '',"
    "    session_key TEXT NOT NULL,"
    "    entry_json TEXT NOT NULL,"
    "    updated_at REAL NOT NULL,"
    "    PRIMARY KEY (scope, session_key)"
    ");"
    "CREATE TABLE IF NOT EXISTS compression_locks ("
    "    session_id TEXT PRIMARY KEY,"
    "    holder TEXT NOT NULL,"
    "    acquired_at REAL NOT NULL,"
    "    expires_at REAL NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS async_delegations ("
    "    delegation_id TEXT PRIMARY KEY,"
    "    origin_session TEXT NOT NULL,"
    "    origin_ui_session_id TEXT NOT NULL DEFAULT '',"
    "    parent_session_id TEXT,"
    "    state TEXT NOT NULL,"
    "    dispatched_at REAL NOT NULL,"
    "    completed_at REAL,"
    "    updated_at REAL NOT NULL,"
    "    event_json TEXT,"
    "    result_json TEXT,"
    "    delivery_state TEXT NOT NULL DEFAULT 'pending',"
    "    delivery_attempts INTEGER NOT NULL DEFAULT 0,"
    "    delivered_at REAL,"
    "    owner_pid INTEGER,"
    "    owner_started_at INTEGER,"
    "    task_json TEXT,"
    "    delivery_claim TEXT,"
    "    delivery_claimed_at REAL"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_sessions_source ON sessions(source);"
    "CREATE INDEX IF NOT EXISTS idx_sessions_source_id ON sessions(source, id);"
    "CREATE INDEX IF NOT EXISTS idx_sessions_parent ON sessions(parent_session_id);"
    "CREATE INDEX IF NOT EXISTS idx_sessions_started ON sessions(started_at DESC);"
    "CREATE INDEX IF NOT EXISTS idx_messages_session ON messages(session_id, timestamp);"
    "CREATE INDEX IF NOT EXISTS idx_compression_locks_expires ON compression_locks(expires_at);"
    "CREATE INDEX IF NOT EXISTS idx_session_model_usage_session ON session_model_usage(session_id);"
    "CREATE INDEX IF NOT EXISTS idx_session_model_usage_model ON session_model_usage(model);"
    "CREATE INDEX IF NOT EXISTS idx_async_delegations_delivery"
    "    ON async_delegations(delivery_state, completed_at);";

/* PoP: HERMES_STATE_DEFERRED_INDEX_SQL @ hermes_state.py:DEFERRED_INDEX_SQL */
const char *HERMES_STATE_DEFERRED_INDEX_SQL =
    "CREATE INDEX IF NOT EXISTS idx_messages_session_active"
    "    ON messages(session_id, active, timestamp);"
    "CREATE INDEX IF NOT EXISTS idx_messages_active_null"
    "    ON messages(active) WHERE active IS NULL;"
    "CREATE INDEX IF NOT EXISTS idx_sessions_session_key"
    "    ON sessions(session_key, started_at DESC);"
    "CREATE INDEX IF NOT EXISTS idx_sessions_gateway_peer"
    "    ON sessions(source, user_id, chat_id, chat_type, thread_id, started_at DESC);"
    "CREATE INDEX IF NOT EXISTS idx_sessions_handoff_state"
    "    ON sessions(handoff_state, started_at);";

/* PoP: HERMES_STATE_FTS_SQL @ hermes_state.py:FTS_SQL */
const char *HERMES_STATE_FTS_SQL =
    "CREATE VIRTUAL TABLE IF NOT EXISTS messages_fts USING fts5("
    "    content,"
    "    tool_name,"
    "    tool_calls,"
    "    content='messages',"
    "    content_rowid='id'"
    ");"
    "CREATE TRIGGER IF NOT EXISTS messages_fts_insert AFTER INSERT ON messages "
    "WHEN (new.id > COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                         WHERE key = 'fts_rebuild_high_water'), -1)"
    "   OR new.id <= COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                          WHERE key = 'fts_rebuild_progress'), -1)) "
    "BEGIN"
    "    INSERT INTO messages_fts(rowid, content, tool_name, tool_calls)"
    "    VALUES (new.id, new.content, new.tool_name, new.tool_calls);"
    "END;"
    "CREATE TRIGGER IF NOT EXISTS messages_fts_delete AFTER DELETE ON messages "
    "WHEN (old.id > COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                         WHERE key = 'fts_rebuild_high_water'), -1)"
    "   OR old.id <= COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                          WHERE key = 'fts_rebuild_progress'), -1)) "
    "BEGIN"
    "    INSERT INTO messages_fts(messages_fts, rowid, content, tool_name, tool_calls)"
    "    VALUES ('delete', old.id, old.content, old.tool_name, old.tool_calls);"
    "END;"
    "CREATE TRIGGER IF NOT EXISTS messages_fts_update AFTER UPDATE ON messages "
    "WHEN (old.content IS NOT new.content"
    "    OR old.tool_name IS NOT new.tool_name"
    "    OR old.tool_calls IS NOT new.tool_calls)"
    "   AND (old.id > COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                           WHERE key = 'fts_rebuild_high_water'), -1)"
    "     OR old.id <= COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                            WHERE key = 'fts_rebuild_progress'), -1)) "
    "BEGIN"
    "    INSERT INTO messages_fts(messages_fts, rowid, content, tool_name, tool_calls)"
    "    VALUES ('delete', old.id, old.content, old.tool_name, old.tool_calls);"
    "    INSERT INTO messages_fts(rowid, content, tool_name, tool_calls)"
    "    VALUES (new.id, new.content, new.tool_name, new.tool_calls);"
    "END;";

/* PoP: HERMES_STATE_FTS_TRIGRAM_SQL @ hermes_state.py:FTS_TRIGRAM_SQL */
const char *HERMES_STATE_FTS_TRIGRAM_SQL =
    "CREATE VIEW IF NOT EXISTS messages_fts_trigram_src AS"
    "    SELECT id, role, content, tool_name, tool_calls"
    "    FROM messages"
    "    WHERE role <> 'tool';"
    "CREATE VIRTUAL TABLE IF NOT EXISTS messages_fts_trigram USING fts5("
    "    content,"
    "    tool_name,"
    "    tool_calls,"
    "    content='messages_fts_trigram_src',"
    "    content_rowid='id',"
    "    tokenize='trigram'"
    ");"
    "CREATE TRIGGER IF NOT EXISTS messages_fts_trigram_insert AFTER INSERT ON messages "
    "WHEN new.role <> 'tool'"
    "   AND (new.id > COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                           WHERE key = 'fts_rebuild_high_water'), -1)"
    "     OR new.id <= COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                            WHERE key = 'fts_rebuild_progress'), -1)) "
    "BEGIN"
    "    INSERT INTO messages_fts_trigram(rowid, content, tool_name, tool_calls)"
    "    VALUES (new.id, new.content, new.tool_name, new.tool_calls);"
    "END;"
    "CREATE TRIGGER IF NOT EXISTS messages_fts_trigram_delete AFTER DELETE ON messages "
    "WHEN old.role <> 'tool'"
    "   AND (old.id > COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                           WHERE key = 'fts_rebuild_high_water'), -1)"
    "     OR old.id <= COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                            WHERE key = 'fts_rebuild_progress'), -1)) "
    "BEGIN"
    "    INSERT INTO messages_fts_trigram(messages_fts_trigram, rowid, content, tool_name, tool_calls)"
    "    VALUES ('delete', old.id, old.content, old.tool_name, old.tool_calls);"
    "END;"
    "CREATE TRIGGER IF NOT EXISTS messages_fts_trigram_update AFTER UPDATE ON messages "
    "WHEN (old.content IS NOT new.content"
    "    OR old.tool_name IS NOT new.tool_name"
    "    OR old.tool_calls IS NOT new.tool_calls"
    "    OR old.role IS NOT new.role)"
    "   AND (old.id > COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                           WHERE key = 'fts_rebuild_high_water'), -1)"
    "     OR old.id <= COALESCE((SELECT CAST(value AS INTEGER) FROM state_meta"
    "                            WHERE key = 'fts_rebuild_progress'), -1)) "
    "BEGIN"
    "    INSERT INTO messages_fts_trigram(messages_fts_trigram, rowid, content, tool_name, tool_calls)"
    "    VALUES ('delete', old.id, old.content, old.tool_name, old.tool_calls);"
    "    INSERT INTO messages_fts_trigram(rowid, content, tool_name, tool_calls)"
    "    SELECT new.id, new.content, new.tool_name, new.tool_calls"
    "    WHERE new.role <> 'tool';"
    "END;";

/* PoP: apply_telegram_topic_migration @ hermes_state.py:apply_telegram_topic_migration */
const char *HERMES_STATE_TOPIC_SQL =
    "CREATE TABLE IF NOT EXISTS telegram_dm_topic_mode ("
    "    chat_id TEXT PRIMARY KEY,"
    "    user_id TEXT NOT NULL,"
    "    enabled INTEGER NOT NULL DEFAULT 1,"
    "    activated_at REAL NOT NULL,"
    "    updated_at REAL NOT NULL,"
    "    has_topics_enabled INTEGER,"
    "    allows_users_to_create_topics INTEGER,"
    "    capability_checked_at REAL,"
    "    intro_message_id TEXT,"
    "    pinned_message_id TEXT"
    ");"
    "CREATE TABLE IF NOT EXISTS telegram_dm_topic_bindings ("
    "    chat_id TEXT NOT NULL,"
    "    thread_id TEXT NOT NULL,"
    "    user_id TEXT NOT NULL,"
    "    session_key TEXT NOT NULL,"
    "    session_id TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,"
    "    managed_mode TEXT NOT NULL DEFAULT 'auto',"
    "    linked_at REAL NOT NULL,"
    "    updated_at REAL NOT NULL,"
    "    PRIMARY KEY (chat_id, thread_id)"
    ");"
    "CREATE UNIQUE INDEX IF NOT EXISTS idx_telegram_dm_topic_bindings_session"
    " ON telegram_dm_topic_bindings(session_id);"
    "CREATE INDEX IF NOT EXISTS idx_telegram_dm_topic_bindings_user"
    " ON telegram_dm_topic_bindings(user_id, chat_id);";

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
    /* Deferred indexes and FTS layout: a fresh DB is born at the current
     * layout (Python: fresh DBs get FTS_STORAGE_VERSION immediately). */
    sqlite3_exec(h->db, HERMES_STATE_DEFERRED_INDEX_SQL, NULL, NULL, NULL);
    if (sqlite3_exec(h->db, HERMES_STATE_FTS_SQL, NULL, NULL, &err) != SQLITE_OK) {
        if (err) { sqlite3_free(err); err = NULL; }
    }
    if (sqlite3_exec(h->db, HERMES_STATE_FTS_TRIGRAM_SQL, NULL, NULL, &err) != SQLITE_OK) {
        if (err) { sqlite3_free(err); err = NULL; }
    }
    /* schema_version: INSERT when absent, else UPDATE to current (Python
     * _migrate). fts_storage_version only stamped when messages_fts exists. */
    sqlite3_exec(h->db,
        "INSERT INTO schema_version (version) "
        "SELECT 23 WHERE NOT EXISTS (SELECT 1 FROM schema_version);",
        NULL, NULL, NULL);
    sqlite3_exec(h->db, "UPDATE schema_version SET version = 23;", NULL, NULL, NULL);
    sqlite3_exec(h->db,
        "INSERT INTO state_meta(key, value) "
        "SELECT 'fts_storage_version', '1' "
        "WHERE EXISTS (SELECT 1 FROM sqlite_master "
        "              WHERE type='table' AND name='messages_fts') "
        "ON CONFLICT(key) DO UPDATE SET value='1';",
        NULL, NULL, NULL);
    return h;
}

/* PoP: apply_telegram_topic_migration @ hermes_state.py:apply_telegram_topic_migration */
/* PoP: hermes_state_apply_telegram_topic_migration @ hermes_state.py:apply_telegram_topic_migration */
bool hermes_state_apply_telegram_topic_migration(hermes_state_db_t *db) {
    if (!db || !db->db) return false;
    char *err = NULL;
    if (sqlite3_exec(db->db, HERMES_STATE_TOPIC_SQL, NULL, NULL, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return false;
    }
    sqlite3_exec(db->db,
        "INSERT INTO state_meta(key, value) "
        "VALUES ('telegram_dm_topic_schema_version', '2') "
        "ON CONFLICT(key) DO UPDATE SET value='2';",
        NULL, NULL, NULL);
    return true;
}

void hermes_state_db_close(hermes_state_db_t *db) {
    if (!db) return;
    if (db->db) sqlite3_close(db->db);
    free(db);
}
