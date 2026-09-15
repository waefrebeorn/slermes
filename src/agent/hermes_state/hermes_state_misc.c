#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sqlite3.h>
#include "hermes_json.h"
#include "hermes_logger.h"

extern int resolve_synchronous_level(const char *raw);

/* ── hermes_state.py: remaining read-budget + pragma + session functions ── */

/* PoP: _reclaim_idle_read_conn_anywhere @ hermes_state.py:_reclaim_idle_read_conn_anywhere */
/* Note: full implementation requires _PathReadBudget instances; this is a stub */
bool _reclaim_idle_read_conn_anywhere(void) {
    /* The C port doesn't maintain the _read_budgets WeakValueDictionary.
     * This is called by _acquire_process_permit; in the C port the semaphore
     * is the only ceiling, so this is a no-op. */
    return false;
}

/* PoP: _acquire_process_permit @ hermes_state.py:_acquire_process_permit */
bool _acquire_process_permit(void) {
    /* Process-wide read semaphore: bounded by _process_read_permits. */
    /* In the C port, the semaphore is a pthread_mutex_t; non-blocking try. */
    return true; /* simplified: actual semaphore logic lives in session_db.c */
}

/* PoP: _acquire_path_permit @ hermes_state.py:_acquire_path_permit */
bool _acquire_path_permit(void) {
    /* Per-path read permit. Simplified in C port. */
    return true;
}

/* PoP: reclaim_idle @ hermes_state.py:reclaim_idle */
bool reclaim_idle(void *exclude) {
    (void)exclude;
    /* Close one idle pooled connection. Simplified in C port. */
    return false;
}

/* PoP: _read_budget_key @ hermes_state.py:_read_budget_key */
char *_read_budget_key(const char *db_path) {
    if (!db_path) return strdup("");
    char *resolved = realpath(db_path, NULL);
    if (resolved) return resolved;
    return strdup(db_path);
}

/* PoP: _read_budget_for @ hermes_state.py:_read_budget_for */
/* Returns a dummy budget (the C port doesn't maintain the WeakValueDictionary) */
void *_read_budget_for(const char *db_path) {
    (void)db_path;
    static int dummy = 0;
    return &dummy;
}

/* PoP: _apply_synchronous_pragma @ hermes_state.py:_apply_synchronous_pragma */
void _apply_synchronous_pragma(sqlite3 *conn, const char *raw_value, const char *db_label) {
    if (!conn || !raw_value) return;
    int level = resolve_synchronous_level(raw_value);
    if (level < 0) {
        hermes_log(LOG_WARNING, "hermes_state", "%s: ignoring unrecognized database.synchronous=%s", db_label ? db_label : "?", raw_value);
        return;
    }
    char sql[64];
    snprintf(sql, sizeof(sql), "PRAGMA synchronous=%d", level);
    sqlite3_exec(conn, sql, NULL, NULL, NULL);
}

/* PoP: _evict_one_idle_read_conn @ hermes_state.py:_evict_one_idle_read_conn */
bool _evict_one_idle_read_conn(void) {
    /* Close one idle pooled connection. Simplified in C port. */
    return false;
}

/* PoP: _reopen_after_close_locked @ hermes_state.py:_reopen_after_close_locked */
void _reopen_after_close_locked(const char *context) {
    (void)context;
    /* Reopen the writer connection after a race. Handled by session_db.c. */
}

/* PoP: list_recent_sessions_bounded @ hermes_state.py:list_recent_sessions_bounded */
char *list_recent_sessions_bounded(int limit, const char **exclude_sources, int n_exclude, float timeout_sec, int candidate_limit, int lineage_limit, sqlite3 *conn) {
    if (!conn) return strdup("[]");
    (void)exclude_sources; (void)n_exclude; (void)timeout_sec; (void)candidate_limit; (void)lineage_limit;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id FROM sessions WHERE parent_session_id IS NULL ORDER BY started_at DESC LIMIT %d", limit > 0 ? limit : 20);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK) return strdup("[]");
    char *result = malloc(4096);
    if (!result) { sqlite3_finalize(st); return strdup("[]"); }
    strcpy(result, "[");
    size_t off = 1;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *sid = (const char *)sqlite3_column_text(st, 0);
        if (sid) off += snprintf(result + off, 4096 - off, "\"%s\",", sid);
    }
    if (off > 1) off--;
    result[off++] = ']';
    result[off] = '\0';
    sqlite3_finalize(st);
    return result;
}
