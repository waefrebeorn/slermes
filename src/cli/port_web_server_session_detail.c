/*
 * port_web_server_session_detail.c — session detail plumbing: a faithful C11
 * port of the SessionDB message/session methods in hermes_state.py
 * (get_session, get_messages, sanitize_title, get_compression_tip,
 * resolve_resume_session_id, delete_session, set/get_session_title,
 * set_session_archived, set_session_pinned, export_session).
 *
 * Self-contained: opens the sqlite store directly (matching the sibling
 * port_web_server_sessions_admin.c pattern), uses libjson for JSON. No
 * dependency on the opaque hermes_state_db_t internals.
 *
 * The web_server.py HTTP-shape wrappers live in
 * port_web_server_session_endpoints.c, which reuses this plumbing through
 * web_server_session_detail.h (no god header, no duplicated SQL).
 */

#include "web_server_session_detail.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "sqlite3.h"

/* ───────────────────────────── shared SQL utils ──────────────────────── */

/* Open the store read-only (rw=false) or read/write. Returns NULL on failure. */
sqlite3 *ws_sess_open_db(const char *path, bool rw) {
    sqlite3 *db = NULL;
    int flags = rw ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;
    if (sqlite3_open_v2(path, &db, flags, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    return db;
}

static const char *col_txt(sqlite3_stmt *st, int i) {
    const unsigned char *t = sqlite3_column_text(st, i);
    return t ? (const char *)t : NULL;
}

/* UTF-8 decode one code point. Returns #bytes consumed (1-4) or 0 on
 * invalid lead; writes the code point to *cp. */
static int utf8_decode(const unsigned char *s, unsigned long *cp) {
    unsigned char c = s[0];
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0) {
        if (!s[1]) return 0;
        *cp = ((unsigned long)(c & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {
        if (!s[1] || !s[2]) return 0;
        *cp = ((unsigned long)(c & 0x0F) << 12) |
              ((unsigned long)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {
        if (!s[1] || !s[2] || !s[3]) return 0;
        *cp = ((unsigned long)(c & 0x07) << 18) |
              ((unsigned long)(s[1] & 0x3F) << 12) |
              ((unsigned long)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    return 0;
}

static int utf8_encode(unsigned long cp, char *out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* ── sanitize_title ─────────────────────────────────────────────────── */
/* PoP: ws_sess_sanitize_title @ hermes_state.py:sanitize_title */
#define MAX_TITLE_LENGTH 100

char *ws_sess_sanitize_title(const char *title, char **err) {
    if (err) *err = NULL;
    if (!title || !*title) return NULL;

    /* Reject lone surrogates / pass-through via a code-point filter. */
    size_t inlen = strlen(title);
    char *clean = malloc(inlen * 4 + 1);
    size_t oi = 0;
    for (size_t i = 0; i < inlen; ) {
        unsigned long cp;
        int n = utf8_decode((const unsigned char *)title + i, &cp);
        if (n == 0) { i++; continue; } /* skip invalid byte */
        i += (size_t)n;
        /* ASCII control chars 0x00-0x1F / 0x7F, but keep \t \n \r
         * (0x09,0x0A,0x0D) so the whitespace collapse can normalize them. */
        if (cp <= 0x1F && cp != 0x09 && cp != 0x0A && cp != 0x0D) continue;
        if (cp == 0x7F) continue;
        /* Problematic Unicode controls. */
        if ((cp >= 0x200B && cp <= 0x200F) ||
            (cp >= 0x2028 && cp <= 0x202E) ||
            (cp >= 0x2060 && cp <= 0x2069) ||
            cp == 0xFEFF || cp == 0xFFFC ||
            (cp >= 0xFFF9 && cp <= 0xFFFB))
            continue;
        /* Lone surrogate range. */
        if (cp >= 0xD800 && cp <= 0xDFFF) continue;
        oi += (size_t)utf8_encode(cp, clean + oi);
    }
    clean[oi] = '\0';

    /* Collapse internal whitespace runs to single space + strip. */
    char *out = malloc(strlen(clean) + 1);
    size_t oj = 0;
    bool prev_ws = true; /* suppress leading whitespace */
    for (size_t i = 0; clean[i]; ) {
        unsigned long cp;
        int n = utf8_decode((const unsigned char *)clean + i, &cp);
        if (n == 0) { i++; continue; }
        i += (size_t)n;
        bool ws = (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' ||
                   cp == '\f' || cp == '\v');
        if (ws) {
            if (!prev_ws) out[oj++] = ' ';
            prev_ws = true;
        } else {
            memcpy(out + oj, clean + i - n, (size_t)n);
            oj += (size_t)n;
            prev_ws = false;
        }
    }
    if (oj > 0 && out[oj - 1] == ' ') oj--; /* strip trailing space */
    out[oj] = '\0';
    free(clean);

    if (oj == 0) { free(out); return NULL; }
    if (oj > MAX_TITLE_LENGTH) {
        free(out);
        if (err) {
            char buf[128];
            int k = snprintf(buf, sizeof buf,
                             "Title too long (%zu chars, max %d)", oj, MAX_TITLE_LENGTH);
            *err = malloc((size_t)k + 1);
            memcpy(*err, buf, (size_t)k + 1);
        }
        return NULL;
    }
    return out;
}

/* ── get_session ────────────────────────────────────────────────────── */
/* PoP: ws_sess_get_session @ hermes_state.py:get_session */
json_t *ws_sess_get_session(const char *db_path, const char *session_id) {
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return NULL;
    sqlite3_stmt *st = NULL;
    json_t *out = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT * FROM sessions WHERE id = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            int ncol = sqlite3_column_count(st);
            out = json_object();
            for (int c = 0; c < ncol; c++) {
                const char *name = sqlite3_column_name(st, c);
                int t = sqlite3_column_type(st, c);
                json_t *v;
                if (t == SQLITE_NULL) v = json_null();
                else if (t == SQLITE_INTEGER)
                    v = json_number((double)sqlite3_column_int64(st, c));
                else if (t == SQLITE_FLOAT)
                    v = json_number(sqlite3_column_double(st, c));
                else
                    v = json_string(col_txt(st, c));
                json_set(out, name, v);
            }
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return out;
}

/* ── get_messages ───────────────────────────────────────────────────── */
/* PoP: ws_sess_get_messages @ hermes_state.py:get_messages */
static json_t *decode_message_row(sqlite3_stmt *st) {
    int ncol = sqlite3_column_count(st);
    json_t *m = json_object();
    for (int c = 0; c < ncol; c++) {
        const char *name = sqlite3_column_name(st, c);
        int t = sqlite3_column_type(st, c);
        json_t *v;
        if (t == SQLITE_NULL) { v = json_null(); }
        else if (t == SQLITE_INTEGER)
            v = json_number((double)sqlite3_column_int64(st, c));
        else if (t == SQLITE_FLOAT)
            v = json_number(sqlite3_column_double(st, c));
        else {
            const char *raw = col_txt(st, c);
            if (strcmp(name, "content") == 0 && raw &&
                strncmp(raw, "\x00json:", 6) == 0) {
                /* _decode_content: JSON-encoded blob. */
                json_t *dec = json_parse(raw + 6, NULL);
                v = dec ? dec : json_string(raw);
            } else if (strcmp(name, "tool_calls") == 0 && raw) {
                json_t *dec = json_parse(raw, NULL);
                v = dec ? dec : json_array();
            } else if (strcmp(name, "display_metadata") == 0 && raw) {
                json_t *dec = json_parse(raw, NULL);
                v = (dec && dec->type == JSON_OBJECT) ? dec : json_null();
            } else {
                v = json_string(raw);
            }
        }
        json_set(m, name, v);
    }
    return m;
}

json_t *ws_sess_get_messages(const char *db_path, const char *session_id,
                             bool include_inactive, bool has_limit,
                             int limit, int offset) {
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return json_array();
    char sql[1024];
    const char *active = include_inactive ? "" : " AND active = 1";
    if (has_limit)
        snprintf(sql, sizeof sql,
                 "SELECT * FROM messages WHERE session_id = ?%s "
                 "ORDER BY id LIMIT ? OFFSET ?", active);
    else
        snprintf(sql, sizeof sql,
                 "SELECT * FROM messages WHERE session_id = ?%s "
                 "ORDER BY id", active);
    sqlite3_stmt *st = NULL;
    json_t *arr = json_array();
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (has_limit) {
            sqlite3_bind_int(st, 2, limit);
            sqlite3_bind_int(st, 3, offset);
        }
        while (sqlite3_step(st) == SQLITE_ROW)
            json_append(arr, decode_message_row(st));
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return arr;
}

/* ── compression_tip / resolve_resume_id ────────────────────────────── */
/* PoP: ws_sess_compression_tip @ hermes_state.py:get_compression_tip */
char *ws_sess_compression_tip(const char *db_path, const char *session_id) {
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return strdup(session_id ? session_id : "");
    char *current = strdup(session_id ? session_id : "");
    char *seen[128];
    int seen_n = 0;
    /* Bounded defensive walk (Python caps at 100). */
    for (int iter = 0; iter < 100; iter++) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db,
                "SELECT child.id FROM sessions parent "
                "JOIN sessions child ON child.parent_session_id = parent.id "
                "WHERE parent.id = ? AND parent.end_reason = 'compression' "
                "AND json_extract(COALESCE(child.model_config,'{}'),'$._branched_from') IS NULL "
                "AND json_extract(COALESCE(child.model_config,'{}'),'$._delegate_from') IS NULL "
                "AND COALESCE(child.source,'') != 'tool' "
                "ORDER BY CASE WHEN child.end_reason='compression' THEN 0 "
                "WHEN child.ended_at IS NULL THEN 1 ELSE 2 END, "
                "child.started_at DESC, child.id DESC LIMIT 1",
                -1, &st, NULL) != SQLITE_OK) { sqlite3_finalize(st); break; }
        sqlite3_bind_text(st, 1, current, -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(st);
        char childbuf[256];
        childbuf[0] = '\0';
        if (rc == SQLITE_ROW) {
            const unsigned char *c = sqlite3_column_text(st, 0);
            if (c) snprintf(childbuf, sizeof childbuf, "%s", (const char *)c);
        }
        sqlite3_finalize(st);   /* child pointer now dangles — use childbuf */
        if (!childbuf[0]) break;
        bool already = false;
        for (int s = 0; s < seen_n; s++)
            if (strcmp(seen[s], childbuf) == 0) already = true;
        if (already) break;
        if (seen_n < 128) seen[seen_n++] = strdup(childbuf);
        free(current);
        current = strdup(childbuf);
    }
    for (int s = 0; s < seen_n; s++) free(seen[s]);
    sqlite3_close(db);
    return current;
}

/* PoP: ws_sess_resolve_resume_id @ hermes_state.py:resolve_resume_session_id */
char *ws_sess_resolve_resume_id(const char *db_path, const char *session_id) {
    if (!session_id || !*session_id) return strdup(session_id ? session_id : "");
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return strdup(session_id);

    /* Step 1: follow compression tip forward. */
    char *tip = ws_sess_compression_tip(db_path, session_id);
    const char *base = (tip && *tip) ? tip : session_id;

    char *current = strdup(base);
    char *best = NULL;
    char *seen[128];
    int seen_n = 0;
    for (int iter = 0; iter < 32; iter++) {
        /* has messages? */
        sqlite3_stmt *st = NULL;
        bool has_msg = false;
        if (sqlite3_prepare_v2(db,
                "SELECT 1 FROM messages WHERE session_id = ? LIMIT 1",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, current, -1, SQLITE_TRANSIENT);
            has_msg = (sqlite3_step(st) == SQLITE_ROW);
        }
        sqlite3_finalize(st);
        if (has_msg) { free(best); best = strdup(current); }

        if (sqlite3_prepare_v2(db,
                "SELECT id FROM sessions WHERE parent_session_id = ? "
                "AND json_extract(COALESCE(model_config,'{}'),'$._branched_from') IS NULL "
                "AND json_extract(COALESCE(model_config,'{}'),'$._delegate_from') IS NULL "
                "AND COALESCE(source,'') != 'tool' "
                "ORDER BY started_at DESC, id DESC LIMIT 1",
                -1, &st, NULL) != SQLITE_OK)
            break;
        sqlite3_bind_text(st, 1, current, -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(st);
        char childbuf[256];
        childbuf[0] = '\0';
        if (rc == SQLITE_ROW) {
            const unsigned char *c = sqlite3_column_text(st, 0);
            if (c) snprintf(childbuf, sizeof childbuf, "%s", (const char *)c);
        }
        sqlite3_finalize(st);   /* child pointer now dangles — use childbuf */
        if (!childbuf[0]) break;
        bool already = false;
        for (int s = 0; s < seen_n; s++)
            if (strcmp(seen[s], childbuf) == 0) already = true;
        if (already) break;
        if (seen_n < 128) seen[seen_n++] = strdup(childbuf);
        free(current);
        current = strdup(childbuf);
    }
    char *result = strdup(best ? best : base);
    free(current); free(best);
    for (int s = 0; s < seen_n; s++) free(seen[s]);
    free(tip);
    sqlite3_close(db);
    return result;
}

/* ── delete_session ─────────────────────────────────────────────────── */
/* PoP: ws_sess_delete_session @ hermes_state.py:delete_session */
bool ws_sess_delete_session(const char *db_path, const char *session_id) {
    sqlite3 *db = ws_sess_open_db(db_path, true);
    if (!db) return false;
    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    sqlite3_stmt *st = NULL;
    int count = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM sessions WHERE id = ?",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    if (count == 0) {
        sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        sqlite3_close(db);
        return false;
    }
    if (sqlite3_prepare_v2(db,
            "UPDATE sessions SET parent_session_id = NULL "
            "WHERE parent_session_id = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    if (sqlite3_prepare_v2(db, "DELETE FROM messages WHERE session_id = ?",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    if (sqlite3_prepare_v2(db, "DELETE FROM sessions WHERE id = ?",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    sqlite3_close(db);
    return true;
}

/* ── set_session_title / get_session_title ──────────────────────────── */
/* PoP: ws_sess_get_title @ hermes_state.py:get_session_title */
char *ws_sess_get_title(const char *db_path, const char *session_id) {
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return NULL;
    sqlite3_stmt *st = NULL;
    char *out = NULL;
    if (sqlite3_prepare_v2(db, "SELECT title FROM sessions WHERE id = ?",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *t = sqlite3_column_text(st, 0);
            if (t) out = strdup((const char *)t);
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    return out; /* may be NULL */
}

/* PoP: ws_sess_set_title @ hermes_state.py:set_session_title */
bool ws_sess_set_title(const char *db_path, const char *session_id,
                       const char *title, char **err) {
    if (err) *err = NULL;
    char *clean = ws_sess_sanitize_title(title, err);
    if (!clean) return false; /* too long or empty */

    sqlite3 *db = ws_sess_open_db(db_path, true);
    if (!db) { free(clean); return false; }
    bool ok = false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT id FROM sessions WHERE title = ? AND id != ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, clean, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *conflict = sqlite3_column_text(st, 0);
            /* Compression-ancestor transfer is a no-op simplification:
             * we just reject the conflict like Python's non-ancestor path. */
            if (err) {
                size_t sz = 64;
                *err = malloc(sz);
                snprintf(*err, sz, "already_in_use:%s",
                         conflict ? (const char *)conflict : "");
            }
            ok = false;
        } else {
            sqlite3_stmt *up = NULL;
            if (sqlite3_prepare_v2(db,
                    "UPDATE sessions SET title = ? WHERE id = ?",
                    -1, &up, NULL) == SQLITE_OK) {
                sqlite3_bind_text(up, 1, clean, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(up, 2, session_id, -1, SQLITE_TRANSIENT);
                sqlite3_step(up);
                ok = sqlite3_changes(db) > 0;
            }
            sqlite3_finalize(up);
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    free(clean);
    return ok;
}

/* ── set_archived / set_pinned ──────────────────────────────────────── */
/* PoP: ws_sess_set_archived @ hermes_state.py:set_session_archived */
bool ws_sess_set_archived(const char *db_path, const char *session_id,
                          bool archived) {
    sqlite3 *db = ws_sess_open_db(db_path, true);
    if (!db) return false;
    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    sqlite3_stmt *st = NULL;
    const char *sql =
        "WITH RECURSIVE "
        "  ancestors(id) AS ("
        "    SELECT ? "
        "    UNION "
        "    SELECT parent.id FROM ancestors a "
        "    JOIN sessions child ON child.id = a.id "
        "    JOIN sessions parent ON parent.id = child.parent_session_id "
        "    WHERE parent.end_reason = 'compression'), "
        "  descendants(id) AS ("
        "    SELECT ? "
        "    UNION "
        "    SELECT child.id FROM descendants d "
        "    JOIN sessions parent ON parent.id = d.id "
        "    JOIN sessions child ON child.parent_session_id = parent.id "
        "    WHERE parent.end_reason = 'compression'), "
        "  lineage(id) AS (SELECT id FROM ancestors UNION SELECT id FROM descendants) "
        "UPDATE sessions SET archived = ? WHERE id IN (SELECT id FROM lineage)";
    int rowcount = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 3, archived ? 1 : 0);
        sqlite3_step(st);
        rowcount = sqlite3_changes(db);
    }
    sqlite3_finalize(st);
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    sqlite3_close(db);
    return rowcount > 0;
}

/* PoP: ws_sess_set_pinned @ hermes_state.py:set_session_pinned */
bool ws_sess_set_pinned(const char *db_path, const char *session_id,
                        bool pinned) {
    sqlite3 *db = ws_sess_open_db(db_path, true);
    if (!db) return false;
    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    sqlite3_stmt *st = NULL;
    const char *sql =
        "WITH RECURSIVE "
        "  ancestors(id) AS ("
        "    SELECT ? "
        "    UNION "
        "    SELECT parent.id FROM ancestors a "
        "    JOIN sessions child ON child.id = a.id "
        "    JOIN sessions parent ON parent.id = child.parent_session_id "
        "    WHERE parent.end_reason = 'compression'), "
        "  descendants(id) AS ("
        "    SELECT ? "
        "    UNION "
        "    SELECT child.id FROM descendants d "
        "    JOIN sessions parent ON parent.id = d.id "
        "    JOIN sessions child ON child.parent_session_id = parent.id "
        "    WHERE parent.end_reason = 'compression'), "
        "  lineage(id) AS (SELECT id FROM ancestors UNION SELECT id FROM descendants) "
        "UPDATE sessions SET pinned = ? WHERE id IN (SELECT id FROM lineage)";
    int rowcount = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 3, pinned ? 1 : 0);
        sqlite3_step(st);
        rowcount = sqlite3_changes(db);
    }
    sqlite3_finalize(st);
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    sqlite3_close(db);
    return rowcount > 0;
}

/* ── export_session ─────────────────────────────────────────────────── */
/* PoP: ws_sess_export_session @ hermes_state.py:export_session */
json_t *ws_sess_export_session(const char *db_path, const char *session_id) {
    json_t *sess = ws_sess_get_session(db_path, session_id);
    if (!sess) return NULL;
    json_t *msgs = ws_sess_get_messages(db_path, session_id, false, false, 0, 0);
    json_set(sess, "messages", msgs);
    return sess;
}
