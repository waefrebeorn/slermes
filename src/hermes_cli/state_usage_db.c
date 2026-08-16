/* port_state_usage_db.c — faithful C11 port of the usage-accounting surface
 * of hermes_state.py (SessionDB): _insert_session_row FK guard,
 * _record_model_usage upsert, record_auxiliary_usage.
 *
 * Backed by the vendored sqlite3 (lib/libdb/sqlite3.c) against the same
 * state.db the TUI app-state subsystem reads. The upsert SQL is the byte
 * mirror of the Python INSERT ... ON CONFLICT DO UPDATE (issue #23270): aux
 * rows accumulate per-(model,provider,base_url,mode,task) without touching
 * the sessions summary row.
 */

#include "state_usage_db.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

struct state_usage_db {
    sqlite3 *db;
};

/* Mirror of the Python DDL (subset needed by the accounting surface: the
 * sessions FK target and the usage table with its composite PK). */
static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS sessions ("
    "    id TEXT PRIMARY KEY,"
    "    source TEXT NOT NULL,"
    "    user_id TEXT,"
    "    chat_id TEXT,"
    "    thread_id TEXT,"
    "    model TEXT,"
    "    model_config TEXT,"
    "    parent_session_id TEXT,"
    "    started_at REAL NOT NULL,"
    "    billing_provider TEXT,"
    "    billing_base_url TEXT,"
    "    billing_mode TEXT"
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
    "CREATE INDEX IF NOT EXISTS idx_session_model_usage_session"
    "    ON session_model_usage(session_id);"
    "CREATE INDEX IF NOT EXISTS idx_session_model_usage_model"
    "    ON session_model_usage(model);";

/* time.time() */
static double now_epoch(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

state_usage_db_t *state_usage_db_open(const char *path) {
    if (!path || !*path) return NULL;
    state_usage_db_t *h = calloc(1, sizeof(*h));
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
    if (sqlite3_exec(h->db, SCHEMA_SQL, NULL, NULL, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        sqlite3_close(h->db);
        free(h);
        return NULL;
    }
    return h;
}

void state_usage_db_close(state_usage_db_t *db) {
    if (!db) return;
    if (db->db) sqlite3_close(db->db);
    free(db);
}

/* PoP: state_usage_insert_session_row @ hermes_state.py:_insert_session_row */
bool state_usage_insert_session_row(state_usage_db_t *db,
                                    const char *session_id,
                                    const char *source) {
    if (!db || !session_id || !*session_id) return false;
    /* The accounting path only needs the INSERT OR IGNORE guard half of the
     * Python upsert (record_auxiliary_usage calls with source="unknown" and
     * no enrichable metadata). */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT OR IGNORE INTO sessions (id, source, started_at)"
            " VALUES (?, ?, ?)", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, source && *source ? source : "unknown", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, now_epoch());
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: state_usage_record_model_usage @ hermes_state.py:_record_model_usage */
bool state_usage_record_model_usage(state_usage_db_t *db,
                                    const char *session_id,
                                    const char *model,
                                    const char *billing_provider,
                                    const char *billing_base_url,
                                    const char *billing_mode,
                                    const char *task,
                                    int api_call_count,
                                    long long input_tokens,
                                    long long output_tokens,
                                    long long cache_read_tokens,
                                    long long cache_write_tokens,
                                    long long reasoning_tokens,
                                    bool has_estimated_cost,
                                    double estimated_cost_usd,
                                    bool has_actual_cost,
                                    double actual_cost_usd,
                                    const char *cost_status,
                                    const char *cost_source) {
    if (!db || !session_id || !*session_id) return false;
    const char *tk = task ? task : "";

    /* Session-route fallback (main-loop rows only, mirror of the Python
     * SELECT + COALESCE-from-session rules). */
    char sess_model[256] = "", sess_provider[256] = "",
         sess_base_url[256] = "", sess_mode[64] = "";
    if (!*tk) {
        sqlite3_stmt *sel = NULL;
        if (sqlite3_prepare_v2(db->db,
                "SELECT model, billing_provider, billing_base_url,"
                " billing_mode FROM sessions WHERE id = ?",
                -1, &sel, NULL) == SQLITE_OK) {
            sqlite3_bind_text(sel, 1, session_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(sel) == SQLITE_ROW) {
                const unsigned char *v;
                if ((v = sqlite3_column_text(sel, 0)))
                    snprintf(sess_model, sizeof(sess_model), "%s", v);
                if ((v = sqlite3_column_text(sel, 1)))
                    snprintf(sess_provider, sizeof(sess_provider), "%s", v);
                if ((v = sqlite3_column_text(sel, 2)))
                    snprintf(sess_base_url, sizeof(sess_base_url), "%s", v);
                if ((v = sqlite3_column_text(sel, 3)))
                    snprintf(sess_mode, sizeof(sess_mode), "%s", v);
            }
            sqlite3_finalize(sel);
        }
    }

    /* Aux-task rows must NOT inherit the session's main-loop route. */
    const char *eff_model, *eff_provider, *eff_base_url, *eff_mode;
    if (*tk) {
        eff_model    = (model && *model) ? model : "unknown";
        eff_provider = (billing_provider && *billing_provider) ? billing_provider : "";
        eff_base_url = (billing_base_url && *billing_base_url) ? billing_base_url : "";
        eff_mode     = (billing_mode && *billing_mode) ? billing_mode : "";
    } else {
        eff_model    = (model && *model) ? model
                     : (*sess_model ? sess_model : "unknown");
        eff_provider = (billing_provider && *billing_provider) ? billing_provider
                     : (*sess_provider ? sess_provider : "");
        eff_base_url = (billing_base_url && *billing_base_url) ? billing_base_url
                     : (*sess_base_url ? sess_base_url : "");
        eff_mode     = (billing_mode && *billing_mode) ? billing_mode
                     : (*sess_mode ? sess_mode : "");
    }

    double now = now_epoch();
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
        "INSERT INTO session_model_usage ("
        "    session_id, model, billing_provider, billing_base_url, billing_mode,"
        "    task, api_call_count, input_tokens, output_tokens,"
        "    cache_read_tokens, cache_write_tokens, reasoning_tokens,"
        "    estimated_cost_usd, actual_cost_usd, cost_status, cost_source,"
        "    first_seen, last_seen"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        " ON CONFLICT(session_id, model, billing_provider, billing_base_url,"
        "             billing_mode, task)"
        " DO UPDATE SET"
        "    api_call_count = api_call_count + excluded.api_call_count,"
        "    input_tokens = input_tokens + excluded.input_tokens,"
        "    output_tokens = output_tokens + excluded.output_tokens,"
        "    cache_read_tokens = cache_read_tokens + excluded.cache_read_tokens,"
        "    cache_write_tokens = cache_write_tokens + excluded.cache_write_tokens,"
        "    reasoning_tokens = reasoning_tokens + excluded.reasoning_tokens,"
        "    estimated_cost_usd = estimated_cost_usd + excluded.estimated_cost_usd,"
        "    actual_cost_usd = actual_cost_usd + excluded.actual_cost_usd,"
        "    cost_status = COALESCE(excluded.cost_status, cost_status),"
        "    cost_source = COALESCE(excluded.cost_source, cost_source),"
        "    last_seen = excluded.last_seen",
        -1, &st, NULL) != SQLITE_OK)
        return false;

    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, eff_model, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, eff_provider, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, eff_base_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, eff_mode, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 6, tk, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 7, api_call_count > 0 ? api_call_count : 0);
    sqlite3_bind_int64(st, 8, input_tokens > 0 ? input_tokens : 0);
    sqlite3_bind_int64(st, 9, output_tokens > 0 ? output_tokens : 0);
    sqlite3_bind_int64(st, 10, cache_read_tokens > 0 ? cache_read_tokens : 0);
    sqlite3_bind_int64(st, 11, cache_write_tokens > 0 ? cache_write_tokens : 0);
    sqlite3_bind_int64(st, 12, reasoning_tokens > 0 ? reasoning_tokens : 0);
    /* Python: float(estimated_cost_usd or 0.0) — None adds 0.0. */
    sqlite3_bind_double(st, 13, has_estimated_cost ? estimated_cost_usd : 0.0);
    sqlite3_bind_double(st, 14, has_actual_cost ? actual_cost_usd : 0.0);
    if (cost_status) sqlite3_bind_text(st, 15, cost_status, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(st, 15);
    if (cost_source) sqlite3_bind_text(st, 16, cost_source, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(st, 16);
    sqlite3_bind_double(st, 17, now);
    sqlite3_bind_double(st, 18, now);

    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: state_usage_record_auxiliary_usage @ hermes_state.py:record_auxiliary_usage */
bool state_usage_record_auxiliary_usage(state_usage_db_t *db,
                                        const char *session_id,
                                        const char *task,
                                        const char *model,
                                        const char *billing_provider,
                                        const char *billing_base_url,
                                        long long input_tokens,
                                        long long output_tokens,
                                        long long cache_read_tokens,
                                        long long cache_write_tokens,
                                        long long reasoning_tokens,
                                        bool has_estimated_cost,
                                        double estimated_cost_usd) {
    /* Python: if not session_id or not task: return */
    if (!db || !session_id || !*session_id || !task || !*task) return false;
    /* FK guard (same INSERT OR IGNORE update_token_counts uses). */
    if (!state_usage_insert_session_row(db, session_id, "unknown")) return false;
    return state_usage_record_model_usage(
        db, session_id, model, billing_provider, billing_base_url,
        /*billing_mode=*/NULL, task, /*api_call_count=*/1,
        input_tokens, output_tokens, cache_read_tokens, cache_write_tokens,
        reasoning_tokens, has_estimated_cost, estimated_cost_usd,
        /*has_actual_cost=*/false, 0.0,
        /*cost_status=*/NULL, /*cost_source=*/NULL);
}

bool state_usage_get_row(state_usage_db_t *db,
                         const char *session_id,
                         const char *model,
                         const char *task,
                         int *api_call_count,
                         long long *input_tokens,
                         long long *output_tokens,
                         long long *reasoning_tokens,
                         double *estimated_cost_usd) {
    if (!db || !session_id || !model) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT api_call_count, input_tokens, output_tokens,"
            " reasoning_tokens, estimated_cost_usd FROM session_model_usage"
            " WHERE session_id = ? AND model = ? AND task = ?",
            -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, model, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, task ? task : "", -1, SQLITE_TRANSIENT);
    bool found = sqlite3_step(st) == SQLITE_ROW;
    if (found) {
        if (api_call_count) *api_call_count = sqlite3_column_int(st, 0);
        if (input_tokens) *input_tokens = sqlite3_column_int64(st, 1);
        if (output_tokens) *output_tokens = sqlite3_column_int64(st, 2);
        if (reasoning_tokens) *reasoning_tokens = sqlite3_column_int64(st, 3);
        if (estimated_cost_usd) *estimated_cost_usd = sqlite3_column_double(st, 4);
    }
    sqlite3_finalize(st);
    return found;
}
