/* session_recovery_orphans.c — placeholder session reconstruction and
 * partial-recovery orphan cleanup. Faithful port of
 * hermes_cli/session_recovery.py (orphans slice).
 */
#define _GNU_SOURCE
#include "session_recovery_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: sr_reconstruct_missing_sessions @ hermes_cli/session_recovery.py:_reconstruct_missing_sessions */
json_t *sr_reconstruct_missing_sessions(sqlite3 *dst) {
    json_t *result = json_object();
    json_set(result, "sessions_reconstructed", json_number(0));
    json_set(result, "messages_retained", json_number(0));

    json_t *sc = sr_table_columns(dst, "sessions");
    json_t *mc = sr_table_columns(dst, "messages");
    bool ok = json_len(sc) > 0 && json_len(mc) > 0;
    json_free(sc); json_free(mc);
    if (!ok) return result;

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(dst,
            "SELECT m.session_id, MIN(m.timestamp), COUNT(*) "
            "FROM messages AS m "
            "WHERE m.session_id IS NOT NULL AND NOT EXISTS ("
            "SELECT 1 FROM sessions WHERE sessions.id = m.session_id) "
            "GROUP BY m.session_id",
            -1, &st, NULL) != SQLITE_OK) {
        if (st) sqlite3_finalize(st);
        return result;
    }

    long long reconstructed = 0, retained = 0;
    long long title_sequence = 1;
    sqlite3_stmt *probe = NULL, *ins = NULL;
    sqlite3_prepare_v2(dst,
        "SELECT 1 FROM sessions WHERE title = ? LIMIT 1", -1, &probe, NULL);
    sqlite3_prepare_v2(dst,
        "INSERT INTO sessions (id, source, started_at, title, message_count) "
        "VALUES (?, 'recovered', ?, ?, ?)", -1, &ins, NULL);

    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *sid = sqlite3_column_text(st, 0);
        double started_at = sqlite3_column_type(st, 1) == SQLITE_NULL
                                ? 0.0 : sqlite3_column_double(st, 1);
        long long message_count = sqlite3_column_int64(st, 2);

        char title[128];
        for (;;) {
            snprintf(title, sizeof(title),
                     "[recovered %lld] session metadata was unreadable",
                     title_sequence);
            title_sequence++;
            sqlite3_bind_text(probe, 1, title, -1, SQLITE_TRANSIENT);
            int rc = sqlite3_step(probe);
            sqlite3_reset(probe);
            sqlite3_clear_bindings(probe);
            if (rc != SQLITE_ROW) break;
        }

        sqlite3_bind_text(ins, 1, (const char *)sid, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(ins, 2, started_at);
        sqlite3_bind_text(ins, 3, title, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(ins, 4, message_count);
        int rc = sqlite3_step(ins);
        sqlite3_reset(ins);
        sqlite3_clear_bindings(ins);
        if (rc != SQLITE_DONE) continue; /* IntegrityError analog: skip */
        reconstructed++;
        retained += message_count;
    }
    sqlite3_finalize(st);
    if (probe) sqlite3_finalize(probe);
    if (ins) sqlite3_finalize(ins);

    json_set(result, "sessions_reconstructed",
             json_number((double)reconstructed));
    json_set(result, "messages_retained", json_number((double)retained));
    return result;
}

static long long sr_count(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st = NULL;
    long long n = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW)
        n = sqlite3_column_int64(st, 0);
    if (st) sqlite3_finalize(st);
    return n;
}

/* PoP: sr_cleanup_partial_orphans @ hermes_cli/session_recovery.py:_cleanup_partial_orphans */
json_t *sr_cleanup_partial_orphans(sqlite3 *dst) {
    json_t *result = json_object();
    json_set(result, "sessions_parent_cleared", json_number(0));
    json_set(result, "sessions_reconstructed", json_number(0));
    json_set(result, "messages_retained", json_number(0));
    json_set(result, "messages_removed", json_number(0));
    json_set(result, "session_model_usage_removed", json_number(0));
    json_set(result, "compression_locks_removed", json_number(0));
    json_set(result, "telegram_dm_topic_bindings_removed", json_number(0));

    sqlite3_exec(dst, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    /* Rebuild owners BEFORE any orphan deletion. */
    json_t *rebuilt = sr_reconstruct_missing_sessions(dst);
    json_set(result, "sessions_reconstructed",
             json_number(json_get_num(rebuilt, "sessions_reconstructed", 0)));
    json_set(result, "messages_retained",
             json_number(json_get_num(rebuilt, "messages_retained", 0)));
    json_free(rebuilt);

    long long parent_count = sr_count(dst,
        "SELECT COUNT(*) FROM sessions AS child "
        "WHERE child.parent_session_id IS NOT NULL "
        "AND NOT EXISTS ("
        "SELECT 1 FROM sessions AS parent "
        "WHERE parent.id = child.parent_session_id)");
    if (parent_count) {
        sqlite3_exec(dst,
            "UPDATE sessions SET parent_session_id = NULL "
            "WHERE parent_session_id IS NOT NULL "
            "AND NOT EXISTS ("
            "SELECT 1 FROM sessions AS parent "
            "WHERE parent.id = sessions.parent_session_id)",
            NULL, NULL, NULL);
    }
    json_set(result, "sessions_parent_cleared",
             json_number((double)parent_count));

    static const struct { const char *table; const char *key; } deps[4] = {
        { "messages", "messages_removed" },
        { "session_model_usage", "session_model_usage_removed" },
        { "compression_locks", "compression_locks_removed" },
        { "telegram_dm_topic_bindings", "telegram_dm_topic_bindings_removed" },
    };
    for (size_t i = 0; i < 4; i++) {
        json_t *cols = sr_table_columns(dst, deps[i].table);
        bool present = json_len(cols) > 0;
        json_free(cols);
        if (!present) continue;
        char sql[512];
        snprintf(sql, sizeof(sql),
                 "SELECT COUNT(*) FROM \"%s\" AS dependent "
                 "WHERE NOT EXISTS ("
                 "SELECT 1 FROM sessions "
                 "WHERE sessions.id = dependent.session_id)", deps[i].table);
        long long orphan_count = sr_count(dst, sql);
        if (orphan_count) {
            snprintf(sql, sizeof(sql),
                     "DELETE FROM \"%s\" "
                     "WHERE NOT EXISTS ("
                     "SELECT 1 FROM sessions "
                     "WHERE sessions.id = \"%s\".session_id)",
                     deps[i].table, deps[i].table);
            sqlite3_exec(dst, sql, NULL, NULL, NULL);
        }
        json_set(result, deps[i].key, json_number((double)orphan_count));
    }
    sqlite3_exec(dst, "COMMIT", NULL, NULL, NULL);

    /* Only destructive/relinking actions belong in this total. */
    double total =
        json_get_num(result, "sessions_parent_cleared", 0) +
        json_get_num(result, "messages_removed", 0) +
        json_get_num(result, "session_model_usage_removed", 0) +
        json_get_num(result, "compression_locks_removed", 0) +
        json_get_num(result, "telegram_dm_topic_bindings_removed", 0);
    json_set(result, "total_removed_or_relinked", json_number(total));
    return result;
}
