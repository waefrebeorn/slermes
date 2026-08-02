/* hermes_state_lineage.c — session lineage / resume-resolution methods of the
 * SessionDB port: get_conversation_root, get_compression_tip,
 * resolve_resume_session_id, get_compression_lineage. Faithful chain-walk
 * mirrors of hermes_state.py (parent walk + compression-continuation walk,
 * excluding branch/delegate/tool children). Self-contained.
 */

#include "hermes_state_internal.h"
#include "hermes_state_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: get_conversation_root @ hermes_state.py:get_conversation_root */
char *hermes_state_get_conversation_root(hermes_state_db_t *db,
                                         const char *session_id) {
    if (!db || !session_id || !*session_id) return strdup(session_id ? session_id : "");
    char *chain[128];
    int n = 0;
    char *current = strdup(session_id);
    for (int i = 0; i < 100 && n < 128; i++) {
        chain[n++] = current;          /* chain owns current from here */
        sqlite3_stmt *st = NULL;
        char *parent = NULL;
        if (sqlite3_prepare_v2(db->db,
                "SELECT parent_session_id FROM sessions WHERE id = ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, current, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char *p = sqlite3_column_text(st, 0);
                if (p && *p) parent = strdup((const char*)p);
            }
            sqlite3_finalize(st);
        }
        if (!parent) break;
        bool seen = false;
        for (int j = 0; j < n; j++)
            if (strcmp(chain[j], parent) == 0) { seen = true; break; }
        if (seen) { free(parent); break; }
        current = parent;
    }
    /* chain[n-1] is the deepest ancestor reached = the conversation root */
    char *root = strdup(chain[n - 1]);
    for (int i = 0; i < n; i++) free(chain[i]);
    return root;
}

/* PoP: get_compression_tip @ hermes_state.py:get_compression_tip */
char *hermes_state_get_compression_tip(hermes_state_db_t *db,
                                       const char *session_id) {
    if (!db || !session_id || !*session_id) return strdup(session_id ? session_id : "");
    char *current = strdup(session_id);
    for (int i = 0; i < 100; i++) {
        sqlite3_stmt *st = NULL;
        char *next = NULL;
        if (sqlite3_prepare_v2(db->db,
                "SELECT child.id FROM sessions parent "
                "JOIN sessions child ON child.parent_session_id = parent.id "
                "WHERE parent.id = ? AND parent.end_reason = 'compression' "
                "AND json_extract(COALESCE(child.model_config,'{}'),'$._branched_from') IS NULL "
                "AND json_extract(COALESCE(child.model_config,'{}'),'$._delegate_from') IS NULL "
                "AND COALESCE(child.source,'') != 'tool' "
                "ORDER BY CASE WHEN child.end_reason='compression' THEN 0 "
                "WHEN child.ended_at IS NULL THEN 1 ELSE 2 END, "
                "COALESCE((SELECT MAX(m.timestamp) FROM messages m WHERE m.session_id=child.id), child.started_at) DESC, "
                "child.started_at DESC, child.id DESC LIMIT 1",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, current, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char *c = sqlite3_column_text(st, 0);
                if (c && *c) next = strdup((const char*)c);
            }
            sqlite3_finalize(st);
        }
        if (!next) break;
        if (strcmp(next, current) == 0) { free(next); break; }
        free(current);
        current = next;
    }
    return current;
}

/* PoP: resolve_resume_session_id @ hermes_state.py:resolve_resume_session_id */
char *hermes_state_resolve_resume_session_id(hermes_state_db_t *db,
                                             const char *session_id) {
    if (!db || !session_id || !*session_id) return strdup(session_id ? session_id : "");
    char *sid = strdup(session_id);
    char *tip = hermes_state_get_compression_tip(db, sid);
    if (tip && strcmp(tip, sid) != 0) { free(sid); sid = tip; }
    else free(tip);

    char *current = sid;
    char *best = NULL;
    for (int i = 0; i < 32; i++) {
        sqlite3_stmt *chk = NULL;
        bool has_msgs = false;
        if (sqlite3_prepare_v2(db->db,
                "SELECT 1 FROM messages WHERE session_id = ? LIMIT 1",
                -1, &chk, NULL) == SQLITE_OK) {
            sqlite3_bind_text(chk, 1, current, -1, SQLITE_TRANSIENT);
            has_msgs = sqlite3_step(chk) == SQLITE_ROW;
            sqlite3_finalize(chk);
        }
        if (has_msgs) { free(best); best = strdup(current); }
        sqlite3_stmt *st = NULL;
        char *child = NULL;
        if (sqlite3_prepare_v2(db->db,
                "SELECT id FROM sessions WHERE parent_session_id = ? "
                "AND json_extract(COALESCE(model_config,'{}'),'$._branched_from') IS NULL "
                "AND json_extract(COALESCE(model_config,'{}'),'$._delegate_from') IS NULL "
                "AND COALESCE(source,'') != 'tool' "
                "ORDER BY started_at DESC, id DESC LIMIT 1",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, current, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char *c = sqlite3_column_text(st, 0);
                if (c && *c) child = strdup((const char*)c);
            }
            sqlite3_finalize(st);
        }
        if (!child) break;
        if (strcmp(child, current) == 0) { free(child); break; }
        if (best && strcmp(child, best) == 0) { free(child); break; }
        free(current);
        current = child;
    }
    char *result = strdup(best ? best : sid);
    free(best);
    if (current != sid) free(current);
    free(sid);
    return result;
}

/* PoP: get_compression_lineage @ hermes_state.py:get_compression_lineage */
char *hermes_state_get_compression_lineage(hermes_state_db_t *db,
                                           const char *session_id) {
    if (!db || !session_id || !*session_id) return strdup("[]");
    char *root = hermes_state_get_conversation_root(db, session_id);
    char *buf = malloc(4096);
    size_t cap = 4096, len = 0;
    len = (size_t)snprintf(buf, cap, "[\"%s\"", root);
    char *cur = strdup(root);
    for (int i = 0; i < 100; i++) {
        sqlite3_stmt *st = NULL;
        char *next = NULL;
        if (sqlite3_prepare_v2(db->db,
                "SELECT child.id FROM sessions parent "
                "JOIN sessions child ON child.parent_session_id = parent.id "
                "WHERE parent.id = ? AND parent.end_reason = 'compression' "
                "AND json_extract(COALESCE(child.model_config,'{}'),'$._branched_from') IS NULL "
                "AND json_extract(COALESCE(child.model_config,'{}'),'$._delegate_from') IS NULL "
                "AND COALESCE(child.source,'') != 'tool' "
                "ORDER BY COALESCE((SELECT MAX(m.timestamp) FROM messages m WHERE m.session_id=child.id), child.started_at) DESC, "
                "child.started_at DESC, child.id DESC LIMIT 1",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, cur, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char *c = sqlite3_column_text(st, 0);
                if (c && *c) next = strdup((const char*)c);
            }
            sqlite3_finalize(st);
        }
        if (!next || strcmp(next, cur) == 0) { if (next) free(next); break; }
        len += (size_t)snprintf(buf + len, cap - len, ",\"%s\"", next);
        free(cur);
        cur = next;
    }
    len += (size_t)snprintf(buf + len, cap - len, "]");
    free(cur); free(root);
    return buf;
}
