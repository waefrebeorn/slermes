/* hermes_state_messages.c — message read/write methods of the SessionDB port:
 * append_message, get_messages_around, get_anchored_view,
 * get_messages_as_conversation. Faithful SQL mirrors of hermes_state.py.
 * Self-contained; depends only on hermes_state_internal.h.
 */

#include "hermes_state_internal.h"
#include "hermes_state_db.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Emit one message row as a JSON object fragment into *buf (growing it).
 * Columns: id, role, content, tool_name, tool_call_id. */
static void emit_message_json(char **buf, size_t *cap, size_t *len, bool first,
                               long long id, const char *role,
                               const char *content, const char *tool_name,
                               const char *tool_call_id) {
    /* JSON-encode content as a quoted string (escape " and \). */
    char enc[2048];
    size_t el = 0;
    enc[el++] = '"';
    if (content) {
        for (const char *p = content; *p && el < sizeof enc - 2; p++) {
            if (*p == '"' || *p == '\\') { enc[el++] = '\\'; }
            enc[el++] = (char)*p;
        }
    }
    enc[el++] = '"';
    enc[el] = '\0';
    char part[4200];
    int n = snprintf(part, sizeof part,
        "%s{\"id\":%lld,\"role\":\"%s\",\"content\":%s,\"tool_name\":%s,"
        "\"tool_call_id\":%s}",
        first ? "" : ",",
        id,
        role ? role : "",
        enc,
        tool_name ? tool_name : "null",
        tool_call_id ? tool_call_id : "null");
    if (*len + (size_t)n + 1 >= *cap) {
        *cap = (*len + (size_t)n + 1) * 2 + 256;
        *buf = realloc(*buf, *cap);
    }
    memcpy(*buf + *len, part, (size_t)n);
    *len += (size_t)n;
    (*buf)[*len] = '\0';
}

/* PoP: append_message @ hermes_state.py:append_message */
long long hermes_state_append_message(hermes_state_db_t *db,
                                      const char *session_id,
                                      const char *role,
                                      const char *content,
                                      const char *tool_name,
                                      const char *tool_call_id,
                                      int token_count) {
    if (!db || !session_id || !*session_id || !role) return -1;
    bool is_tool = (strcmp(role, "tool") == 0);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT INTO messages (session_id, role, content, tool_name, "
            "tool_call_id, token_count, timestamp, active) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, 1)", -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, role, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, content ? content : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, tool_name ? tool_name : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, tool_call_id ? tool_call_id : "", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 6, token_count);
    sqlite3_bind_double(st, 7, hermes_state_now_epoch());
    if (sqlite3_step(st) != SQLITE_DONE) { sqlite3_finalize(st); return -1; }
    long long rowid = sqlite3_last_insert_rowid(db->db);
    sqlite3_finalize(st);
    sqlite3_stmt *up = NULL;
    const char *sql = is_tool
        ? "UPDATE sessions SET message_count = message_count + 1, "
          "tool_call_count = tool_call_count + 1 WHERE id = ?"
        : "UPDATE sessions SET message_count = message_count + 1 WHERE id = ?";
    if (sqlite3_prepare_v2(db->db, sql, -1, &up, NULL) == SQLITE_OK) {
        sqlite3_bind_text(up, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(up);
        sqlite3_finalize(up);
    }
    return rowid;
}

/* PoP: get_messages_around @ hermes_state.py:get_messages_around */
char *hermes_state_get_messages_around(hermes_state_db_t *db,
                                       const char *session_id,
                                       long long around_message_id,
                                       int window) {
    if (!db || !session_id || !*session_id) return NULL;
    if (window < 0) window = 0;
    sqlite3_stmt *a = NULL;
    bool exists = false;
    if (sqlite3_prepare_v2(db->db,
            "SELECT 1 FROM messages WHERE id = ? AND session_id = ? LIMIT 1",
            -1, &a, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(a, 1, around_message_id);
        sqlite3_bind_text(a, 2, session_id, -1, SQLITE_TRANSIENT);
        exists = sqlite3_step(a) == SQLITE_ROW;
        sqlite3_finalize(a);
    }
    if (!exists)
        return strdup("{\"window\":[],\"messages_before\":0,\"messages_after\":0}");

    char *buf = malloc(4096);
    size_t cap = 4096, len = 0;
    len = (size_t)snprintf(buf, cap, "{\"window\":[");
    bool first = true;
    /* before rows (DESC window+1) then emit reversed */
    sqlite3_stmt *b = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id, role, content, tool_name, tool_call_id FROM messages "
            "WHERE session_id = ? AND id <= ? ORDER BY id DESC LIMIT ?",
            -1, &b, NULL) == SQLITE_OK) {
        sqlite3_bind_text(b, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(b, 2, around_message_id);
        sqlite3_bind_int(b, 3, window + 1);
        long long *ids = malloc(sizeof(long long) * (window + 1));
        int cnt = 0;
        while (sqlite3_step(b) == SQLITE_ROW) ids[cnt++] = sqlite3_column_int64(b, 0);
        sqlite3_finalize(b);
        for (int i = cnt - 1; i >= 0; i--) {
            sqlite3_stmt *r = NULL;
            if (sqlite3_prepare_v2(db->db,
                    "SELECT id, role, content, tool_name, tool_call_id "
                    "FROM messages WHERE id = ?", -1, &r, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(r, 1, ids[i]);
                if (sqlite3_step(r) == SQLITE_ROW)
                    emit_message_json(&buf, &cap, &len, first,
                        sqlite3_column_int64(r, 0),
                        (const char*)sqlite3_column_text(r, 1),
                        (const char*)sqlite3_column_text(r, 2),
                        (const char*)sqlite3_column_text(r, 3),
                        (const char*)sqlite3_column_text(r, 4));
                sqlite3_finalize(r);
                first = false;
            }
        }
        free(ids);
    }
    /* after rows (ASC window) */
    sqlite3_stmt *af = NULL;
    int after = 0;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id, role, content, tool_name, tool_call_id FROM messages "
            "WHERE session_id = ? AND id > ? ORDER BY id ASC LIMIT ?",
            -1, &af, NULL) == SQLITE_OK) {
        sqlite3_bind_text(af, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(af, 2, around_message_id);
        sqlite3_bind_int(af, 3, window);
        while (sqlite3_step(af) == SQLITE_ROW) {
            emit_message_json(&buf, &cap, &len, first,
                sqlite3_column_int64(af, 0),
                (const char*)sqlite3_column_text(af, 1),
                (const char*)sqlite3_column_text(af, 2),
                (const char*)sqlite3_column_text(af, 3),
                (const char*)sqlite3_column_text(af, 4));
            first = false; after++;
        }
        sqlite3_finalize(af);
    }
    len += (size_t)snprintf(buf + len, cap - len, "],\"messages_before\":%d,"
                            "\"messages_after\":%d}", window, after);
    return buf;
}

/* PoP: get_anchored_view @ hermes_state.py:get_anchored_view */
char *hermes_state_get_anchored_view(hermes_state_db_t *db,
                                     const char *session_id,
                                     long long around_message_id,
                                     int window, int bookend) {
    if (!db || !session_id || !*session_id) return NULL;
    if (window < 0) window = 0;
    if (bookend < 0) bookend = 0;
    sqlite3_stmt *a = NULL;
    bool exists = false;
    if (sqlite3_prepare_v2(db->db,
            "SELECT 1 FROM messages WHERE id = ? AND session_id = ? LIMIT 1",
            -1, &a, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(a, 1, around_message_id);
        sqlite3_bind_text(a, 2, session_id, -1, SQLITE_TRANSIENT);
        exists = sqlite3_step(a) == SQLITE_ROW;
        sqlite3_finalize(a);
    }
    if (!exists)
        return strdup("{\"window\":[],\"messages_before\":0,\"messages_after\":0,"
                      "\"bookend_start\":[],\"bookend_end\":[]}");

    char *buf = malloc(8192);
    size_t cap = 8192, len = 0;
    len = (size_t)snprintf(buf, cap, "{\"window\":[");
    bool first = true;
    const long long anchor = around_message_id;

    /* window before (DESC window+1) reversed */
    sqlite3_stmt *b = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id, role, content, tool_name, tool_call_id FROM messages "
            "WHERE session_id = ? AND id <= ? ORDER BY id DESC LIMIT ?",
            -1, &b, NULL) == SQLITE_OK) {
        sqlite3_bind_text(b, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(b, 2, anchor);
        sqlite3_bind_int(b, 3, window + 1);
        long long *ids = malloc(sizeof(long long) * (window + 1));
        int cnt = 0;
        while (sqlite3_step(b) == SQLITE_ROW) ids[cnt++] = sqlite3_column_int64(b, 0);
        sqlite3_finalize(b);
        for (int i = cnt - 1; i >= 0; i--) {
            sqlite3_stmt *r = NULL;
            if (sqlite3_prepare_v2(db->db,
                    "SELECT id, role, content, tool_name, tool_call_id "
                    "FROM messages WHERE id = ?", -1, &r, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(r, 1, ids[i]);
                if (sqlite3_step(r) == SQLITE_ROW) {
                    const char *role = (const char*)sqlite3_column_text(r, 1);
                    bool is_anchor = (ids[i] == anchor);
                    if (is_anchor || strcmp(role, "user") == 0 ||
                        strcmp(role, "assistant") == 0)
                        emit_message_json(&buf, &cap, &len, first,
                            sqlite3_column_int64(r, 0), role,
                            (const char*)sqlite3_column_text(r, 2),
                            (const char*)sqlite3_column_text(r, 3),
                            (const char*)sqlite3_column_text(r, 4));
                    first = false;
                }
                sqlite3_finalize(r);
            }
        }
        free(ids);
    }
    /* window after (ASC window) */
    sqlite3_stmt *af = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id, role, content, tool_name, tool_call_id FROM messages "
            "WHERE session_id = ? AND id > ? ORDER BY id ASC LIMIT ?",
            -1, &af, NULL) == SQLITE_OK) {
        sqlite3_bind_text(af, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(af, 2, anchor);
        sqlite3_bind_int(af, 3, window);
        while (sqlite3_step(af) == SQLITE_ROW) {
            const char *role = (const char*)sqlite3_column_text(af, 1);
            if (strcmp(role, "user") == 0 || strcmp(role, "assistant") == 0)
                emit_message_json(&buf, &cap, &len, first,
                    sqlite3_column_int64(af, 0), role,
                    (const char*)sqlite3_column_text(af, 2),
                    (const char*)sqlite3_column_text(af, 3),
                    (const char*)sqlite3_column_text(af, 4));
            first = false;
        }
        sqlite3_finalize(af);
    }
    len += (size_t)snprintf(buf + len, cap - len, "],\"messages_before\":%d,"
                            "\"messages_after\":%d,\"bookend_start\":[", window, window);
    /* bookend_start: first `bookend` user/assistant rows with id < anchor */
    sqlite3_stmt *bs = NULL;
    first = true;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id, role, content, tool_name, tool_call_id FROM messages "
            "WHERE session_id = ? AND id < ? AND role IN ('user','assistant') "
            "ORDER BY id ASC LIMIT ?", -1, &bs, NULL) == SQLITE_OK) {
        sqlite3_bind_text(bs, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(bs, 2, anchor);
        sqlite3_bind_int(bs, 3, bookend);
        while (sqlite3_step(bs) == SQLITE_ROW) {
            emit_message_json(&buf, &cap, &len, first,
                sqlite3_column_int64(bs, 0),
                (const char*)sqlite3_column_text(bs, 1),
                (const char*)sqlite3_column_text(bs, 2),
                (const char*)sqlite3_column_text(bs, 3),
                (const char*)sqlite3_column_text(bs, 4));
            first = false;
        }
        sqlite3_finalize(bs);
    }
    len += (size_t)snprintf(buf + len, cap - len, "],\"bookend_end\":[");
    /* bookend_end: last `bookend` user/assistant rows with id > anchor */
    sqlite3_stmt *be = NULL;
    first = true;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id, role, content, tool_name, tool_call_id FROM messages "
            "WHERE session_id = ? AND id > ? AND role IN ('user','assistant') "
            "ORDER BY id DESC LIMIT ?", -1, &be, NULL) == SQLITE_OK) {
        sqlite3_bind_text(be, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(be, 2, anchor);
        sqlite3_bind_int(be, 3, bookend);
        long long *ids = malloc(sizeof(long long) * (bookend + 1));
        int cnt = 0;
        while (sqlite3_step(be) == SQLITE_ROW) ids[cnt++] = sqlite3_column_int64(be, 0);
        sqlite3_finalize(be);
        for (int i = cnt - 1; i >= 0; i--) {
            sqlite3_stmt *r = NULL;
            if (sqlite3_prepare_v2(db->db,
                    "SELECT id, role, content, tool_name, tool_call_id "
                    "FROM messages WHERE id = ?", -1, &r, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(r, 1, ids[i]);
                if (sqlite3_step(r) == SQLITE_ROW)
                    emit_message_json(&buf, &cap, &len, first,
                        sqlite3_column_int64(r, 0),
                        (const char*)sqlite3_column_text(r, 1),
                        (const char*)sqlite3_column_text(r, 2),
                        (const char*)sqlite3_column_text(r, 3),
                        (const char*)sqlite3_column_text(r, 4));
                sqlite3_finalize(r);
                first = false;
            }
        }
        free(ids);
    }
    len += (size_t)snprintf(buf + len, cap - len, "]}");
    return buf;
}

/* PoP: get_messages_as_conversation @ hermes_state.py:get_messages_as_conversation */
char *hermes_state_get_messages_as_conversation(hermes_state_db_t *db,
                                                const char *session_id,
                                                bool include_inactive) {
    if (!db || !session_id || !*session_id) return strdup("[]");
    const char *active_clause = include_inactive ? "" : " AND active = 1";
    char sql[512];
    snprintf(sql, sizeof sql,
        "SELECT id, role, content FROM messages WHERE session_id = ?%s "
        "ORDER BY id", active_clause);
    sqlite3_stmt *st = NULL;
    char *buf = malloc(4096);
    size_t cap = 4096, len = 0;
    len = (size_t)snprintf(buf, cap, "[");
    bool first = true;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            long long id = sqlite3_column_int64(st, 0);
            const char *role = (const char*)sqlite3_column_text(st, 1);
            const char *content = (const char*)sqlite3_column_text(st, 2);
            /* JSON-encode content as a quoted string (escape " and \). */
            char enc[2048];
            size_t el = 0;
            enc[el++] = '"';
            if (content) {
                for (const char *p = content; *p && el < sizeof enc - 2; p++) {
                    if (*p == '"' || *p == '\\') { enc[el++] = '\\'; }
                    enc[el++] = (char)*p;
                }
            }
            enc[el++] = '"';
            enc[el] = '\0';
            char part[2304];
            int n = snprintf(part, sizeof part,
                "%s{\"id\":%lld,\"role\":\"%s\",\"content\":%s}",
                first ? "" : ",", id, role ? role : "", enc);
            if (len + (size_t)n + 1 >= cap) { cap = len + (size_t)n + 1 + 256; buf = realloc(buf, cap); }
            memcpy(buf + len, part, (size_t)n); len += (size_t)n; buf[len] = '\0';
            first = false;
        }
        sqlite3_finalize(st);
    }
    len += (size_t)snprintf(buf + len, cap - len, "]");
    return buf;
}

/* ── messages cluster (get/around/rewind/restore/recent/clear) ─────────── */

static json_t *hs_msg_to_json(sqlite3_stmt *st) {
    json_t *o = json_object();
    for (int i = 0; i < sqlite3_column_count(st); i++) {
        const char *col = sqlite3_column_name(st, i);
        const unsigned char *v = sqlite3_column_text(st, i);
        json_set(o, col, v ? json_string((const char *)v) : json_null());
    }
    return o;
}

/* PoP: hermes_state_get_messages @ hermes_state.py:get_messages */
char *hermes_state_get_messages(hermes_state_db_t *db, const char *session_id,
                                bool include_inactive, long long limit, long long offset) {
    if (!db || !session_id) return strdup("[]");
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM messages WHERE session_id = ?1 %s ORDER BY id ASC %s%s",
        include_inactive ? "" : "AND active = 1",
        limit > 0 ? "LIMIT ?2 " : "",
        offset > 0 ? "OFFSET ?3" : "");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK) return strdup("[]");
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    int idx = 2;
    if (limit > 0) sqlite3_bind_int64(st, idx++, limit);
    if (offset > 0) sqlite3_bind_int64(st, idx++, offset);
    json_t *arr = json_array();
    while (sqlite3_step(st) == SQLITE_ROW) json_append(arr, hs_msg_to_json(st));
    sqlite3_finalize(st);
    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

/* PoP: hermes_state_get_messages_around @ hermes_state.py:get_messages_around */

/* PoP: hermes_state_rewind_to_message @ hermes_state.py:rewind_to_message */
char *hermes_state_rewind_to_message(hermes_state_db_t *db, const char *session_id,
                                     long long target_message_id) {
    /* Soft-delete all messages with id >= target (the target becomes
     * inactive too) for audit; returns {"rewound_count": N}. */
    if (!db || !session_id) return strdup("{\"rewound_count\":0}");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE messages SET active = 0 WHERE session_id = ?1 AND id >= ?2 AND active = 1",
            -1, &st, NULL) != SQLITE_OK) return strdup("{\"rewound_count\":0}");
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, target_message_id);
    sqlite3_step(st);
    long long n = sqlite3_changes(db->db);
    sqlite3_finalize(st);
    char *out = malloc(64);
    snprintf(out, 64, "{\"rewound_count\":%lld}", n);
    return out;
}

/* PoP: hermes_state_restore_rewound @ hermes_state.py:restore_rewound */
long long hermes_state_restore_rewound(hermes_state_db_t *db, const char *session_id,
                                       long long since_message_id) {
    if (!db || !session_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE messages SET active = 1 WHERE session_id = ?1 AND id >= ?2 AND active = 0",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, since_message_id);
    sqlite3_step(st);
    long long n = sqlite3_changes(db->db);
    sqlite3_finalize(st);
    return n;
}

/* PoP: hermes_state_list_recent_user_messages @ hermes_state.py:list_recent_user_messages */
char *hermes_state_list_recent_user_messages(hermes_state_db_t *db, const char *session_id,
                                             long long limit, bool include_inactive) {
    /* Newest-first user messages; preview = first 80 chars of content with
     * line breaks collapsed to spaces. */
    if (!db || !session_id) return strdup("[]");
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT id, timestamp, content FROM messages "
        "WHERE session_id = ?1 AND role = 'user' %s ORDER BY id DESC LIMIT ?2",
        include_inactive ? "" : "AND active = 1");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK) return strdup("[]");
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, limit > 0 ? limit : 20);
    json_t *arr = json_array();
    while (sqlite3_step(st) == SQLITE_ROW) {
        json_t *o = json_object();
        json_set(o, "id", json_int(sqlite3_column_int64(st, 0)));
        json_set(o, "timestamp", json_int(sqlite3_column_int64(st, 1)));
        const unsigned char *c = sqlite3_column_text(st, 2);
        const char *src = c ? (const char *)c : "";
        char preview[96];
        size_t w = 0;
        bool last_ws = false;
        for (size_t i = 0; src[i] && w < 79; i++) {
            char ch = src[i];
            if (ch == '\n' || ch == '\r' || ch == '\t') { last_ws = true; continue; }
            if (last_ws && w > 0) preview[w++] = ' ';
            last_ws = false;
            preview[w++] = ch;
        }
        while (w > 0 && preview[w-1] == ' ') w--;
        preview[w] = '\0';
        json_set(o, "preview", json_string(preview));
        json_append(arr, o);
    }
    sqlite3_finalize(st);
    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

/* PoP: hermes_state_clear_messages @ hermes_state.py:clear_messages */
void hermes_state_clear_messages(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return;
    sqlite3_stmt *d = NULL;
    if (sqlite3_prepare_v2(db->db, "DELETE FROM messages WHERE session_id = ?", -1, &d, NULL) == SQLITE_OK) {
        sqlite3_bind_text(d, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(d);
        sqlite3_finalize(d);
    }
    sqlite3_stmt *u = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET message_count = 0, tool_call_count = 0 WHERE id = ?", -1, &u, NULL) == SQLITE_OK) {
        sqlite3_bind_text(u, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(u);
        sqlite3_finalize(u);
    }
}
