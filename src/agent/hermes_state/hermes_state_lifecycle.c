/* hermes_state_lifecycle.c — session lifecycle methods of the SessionDB port:
 * create_session, end_session, set_session_archived, get_session.
 * Faithful SQL mirrors of hermes_state.py. Self-contained; depends only on
 * hermes_state_internal.h.
 */

#include "hermes_state_internal.h"
#include "hermes_state_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: create_session @ hermes_state.py:create_session */
bool hermes_state_create_session(hermes_state_db_t *db, const char *session_id,
                                 const char *source) {
    if (!db || !session_id || !*session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT OR IGNORE INTO sessions (id, source, started_at) "
            "VALUES (?, ?, ?)", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, source && *source ? source : "unknown", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, hermes_state_now_epoch());
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: end_session @ hermes_state.py:end_session */
/* PoP: hermes_state_end_session @ hermes_state.py:end_session */
bool hermes_state_end_session(hermes_state_db_t *db, const char *session_id,
                              const char *end_reason) {
    if (!db || !session_id || !*session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET ended_at = ?, end_reason = ? "
            "WHERE id = ? AND ended_at IS NULL", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_double(st, 1, hermes_state_now_epoch());
    sqlite3_bind_text(st, 2, end_reason && *end_reason ? end_reason : "", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, session_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* set_session_archived lives in hermes_state_archive.c — the faithful
 * version flips the WHOLE compression lineage via a recursive CTE. */

/* PoP: (model_config setter) @ hermes_state.py compression-walk exclusion */
bool hermes_state_set_model_config(hermes_state_db_t *db, const char *session_id,
                                   const char *model_config_json) {
    if (!db || !session_id || !*session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET model_config = ? WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, model_config_json ? model_config_json : "{}", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
bool hermes_state_link_child(hermes_state_db_t *db, const char *child_id,
                             const char *parent_id, const char *end_reason) {
    if (!db || !child_id || !*child_id || !parent_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET parent_session_id = ?, end_reason = ?, "
            "ended_at = ? WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, parent_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, end_reason && *end_reason ? end_reason : "", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, hermes_state_now_epoch());
    sqlite3_bind_text(st, 4, child_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: get_session @ hermes_state.py:get_session */
char *hermes_state_get_session(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id || !*session_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id, source, parent_session_id, title, archived, pinned, "
            "started_at, ended_at, end_reason, message_count, model, "
            "input_tokens, output_tokens, reasoning_tokens, estimated_cost_usd "
            "FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *id = sqlite3_column_text(st, 0);
        const unsigned char *src = sqlite3_column_text(st, 1);
        const unsigned char *par = sqlite3_column_text(st, 2);
        const unsigned char *title = sqlite3_column_text(st, 3);
        int arch = sqlite3_column_int(st, 4);
        int pinned = sqlite3_column_int(st, 5);
        double started = sqlite3_column_double(st, 6);
        int ended_null = sqlite3_column_type(st, 7) == SQLITE_NULL;
        const unsigned char *ereason = sqlite3_column_text(st, 8);
        int mc = sqlite3_column_int(st, 9);
        const unsigned char *model = sqlite3_column_text(st, 10);
        long long in = sqlite3_column_int64(st, 11);
        long long out_tok = sqlite3_column_int64(st, 12);
        long long rzn = sqlite3_column_int64(st, 13);
        double cost = sqlite3_column_double(st, 14);
        /* quote-or-null helpers for nullable text columns */
        char parq[512], titleq[1024], ereasonq[256], modelq[512];
        if (par)     snprintf(parq, sizeof parq, "\"%s\"", (const char*)par);     else snprintf(parq, sizeof parq, "null");
        if (title)   snprintf(titleq, sizeof titleq, "\"%s\"", (const char*)title); else snprintf(titleq, sizeof titleq, "null");
        if (ereason) snprintf(ereasonq, sizeof ereasonq, "\"%s\"", (const char*)ereason); else snprintf(ereasonq, sizeof ereasonq, "null");
        if (model)   snprintf(modelq, sizeof modelq, "\"%s\"", (const char*)model); else snprintf(modelq, sizeof modelq, "null");
        int n = snprintf(NULL, 0,
            "{\"id\":\"%s\",\"source\":\"%s\",\"parent_session_id\":%s,"
            "\"title\":%s,\"archived\":%d,\"pinned\":%d,\"started_at\":%.6f,"
            "\"ended_at\":%s,\"end_reason\":%s,\"message_count\":%d,"
            "\"model\":%s,\"input_tokens\":%lld,\"output_tokens\":%lld,"
            "\"reasoning_tokens\":%lld,\"estimated_cost_usd\":%.6f}",
            id ? (const char*)id : "",
            src ? (const char*)src : "",
            parq,
            titleq,
            arch, pinned, started,
            ended_null ? "null" : "0.0",
            ereasonq,
            mc,
            modelq,
            in, out_tok, rzn, cost);
        out = malloc((size_t)n + 1);
        snprintf(out, (size_t)n + 1,
            "{\"id\":\"%s\",\"source\":\"%s\",\"parent_session_id\":%s,"
            "\"title\":%s,\"archived\":%d,\"pinned\":%d,\"started_at\":%.6f,"
            "\"ended_at\":%s,\"end_reason\":%s,\"message_count\":%d,"
            "\"model\":%s,\"input_tokens\":%lld,\"output_tokens\":%lld,"
            "\"reasoning_tokens\":%lld,\"estimated_cost_usd\":%.6f}",
            id ? (const char*)id : "",
            src ? (const char*)src : "",
            parq,
            titleq,
            arch, pinned, started,
            ended_null ? "null" : "0.0",
            ereasonq,
            mc,
            modelq,
            in, out_tok, rzn, cost);
    }
    sqlite3_finalize(st);
    return out;
}
