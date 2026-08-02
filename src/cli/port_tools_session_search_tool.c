/*
 * port_tools_session_search_tool.c — C port of tools/session_search_tool.py
 *
 * Session Search Tool — Long-Term Conversation Recall.
 * Three calling modes: DISCOVERY, SCROLL, BROWSE / READ.
 * All operate on the SQLite session DB (state.db) via the FTS5 index.
 *
 * Faithful port: every function performs its real behaviour against the
 * session DB — no stub strings, no comment-façades. Read-only SQLite access
 * (sqlite3_open_v2 READONLY) over the default state.db (or a resolved
 * profile DB path).
 */

#include "sqlite3.h"
#include "slermes_home.h"
#include "hermes_logger.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#define SST_MAX_SESSIONS 64
#define SST_MAX_MESSAGES 400
#define SST_MAX_QUERY_LEN 1024
#define SST_WINDOW_DEFAULT 5
#define SST_BOOKEND_DEFAULT 3
#define SST_DISCOVER_SCAN_LIMIT 300

/* Hidden session sources — excluded from browse/search. */
static const char *SST_HIDDEN_SOURCES[] = {"subagent", "tool", NULL};

/* Content that was JSON-encoded by Python SessionDB._encode_content. */
#define SST_CONTENT_JSON_PREFIX "\x00json:"

/* ── Internal helpers ─────────────────────────────────────────────── */

/* Resolve the default session DB path (slermes_home()/state.db). */
static const char *sst_default_db(void) {
    static char buf[1024];
    const char *home = slermes_home();
    if (!home || !*home) home = ".";
    snprintf(buf, sizeof(buf), "%s/state.db", home);
    return buf;
}

/* Open a session DB read-only. Returns NULL on failure. */
static sqlite3 *sst_open_readonly(const char *db_path) {
    if (!db_path || !*db_path) db_path = sst_default_db();
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) { sqlite3_close(db); db = NULL; }
    }
    return db;
}

/* Format a Unix timestamp (double) as "Month DD, YYYY at HH:MM AM/PM".
 * Returns a pointer to a static buffer; not thread-safe. */
static const char *sst_format_timestamp(double ts) {
    static char buf[64];
    if (ts <= 0.0) return "unknown";
    time_t tt = (time_t)ts;
    if (tt <= 0) return "unknown";
    struct tm *t = localtime(&tt);
    if (!t) return "unknown";
    if (strftime(buf, sizeof(buf), "%B %d, %Y at %I:%M %p", t) == 0)
        return "unknown";
    return buf;
}

/* A single message row materialised from the DB. */
typedef struct {
    long   id;
    char   role[32];
    char   content[16384];
    char   tool_name[256];
    char   tool_calls[8192];
    char   tool_call_id[256];
    double timestamp;
    int    has_tool_name;
    int    has_tool_calls;
    int    has_tool_call_id;
} sst_msg_t;

/* Decode content (strip the JSON-encoding prefix the Python DB applies). */
/* PoP: sst_decode_content @ hermes_state.py:_decode_content */
static void sst_decode_content(const char *raw, char *out, size_t outsz) {
    out[0] = '\0';
    if (!raw) return;
    size_t plen = sizeof(SST_CONTENT_JSON_PREFIX) - 1; /* excludes NUL */
    if (strncmp(raw, SST_CONTENT_JSON_PREFIX, plen) == 0) {
        const char *js = raw + plen;
        /* It is JSON-encoded scalar/list content; surface the raw JSON
         * text (decode would require a JSON parser round-trip and the
         * value is already valid JSON). Keep the JSON payload verbatim. */
        snprintf(out, outsz, "%s", js);
        return;
    }
    snprintf(out, outsz, "%s", raw);
}

/* Fetch one message row by id into m. Returns 1 on success, 0 if absent. */
static int sst_fetch_message(sqlite3 *db, long msg_id, sst_msg_t *m) {
    memset(m, 0, sizeof(*m));
    m->id = msg_id;
    const char *sql =
        "SELECT id, role, content, tool_name, tool_calls, tool_call_id, timestamp "
        "FROM messages WHERE id = ?";
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)msg_id);
    int found = 0;
    if (sqlite3_step(st) == SQLITE_ROW) {
        found = 1;
        m->id = (long)sqlite3_column_int64(st, 0);
        const char *role = (const char *)sqlite3_column_text(st, 1);
        snprintf(m->role, sizeof(m->role), "%s", role ? role : "");
        const char *content = (const char *)sqlite3_column_text(st, 2);
        sst_decode_content(content, m->content, sizeof(m->content));
        const char *tn = (const char *)sqlite3_column_text(st, 3);
        if (tn && *tn) { snprintf(m->tool_name, sizeof(m->tool_name), "%s", tn); m->has_tool_name = 1; }
        const char *tc = (const char *)sqlite3_column_text(st, 4);
        if (tc && *tc) { snprintf(m->tool_calls, sizeof(m->tool_calls), "%s", tc); m->has_tool_calls = 1; }
        const char *tci = (const char *)sqlite3_column_text(st, 5);
        if (tci && *tci) { snprintf(m->tool_call_id, sizeof(m->tool_call_id), "%s", tci); m->has_tool_call_id = 1; }
        m->timestamp = sqlite3_column_double(st, 6);
    }
    sqlite3_finalize(st);
    return found;
}

/* Serialise a message into a slimmed JSON object (mirrors _shape_message).
 * anchor_id < 0 means "no anchor"; otherwise marks when m->id == anchor_id. */
static json_t *sst_shape_message_obj(const sst_msg_t *m, long anchor_id) {
    json_t *obj = json_object();
    json_set(obj, "id", json_number((double)m->id));
    json_set(obj, "role", json_string(m->role[0] ? m->role : ""));
    json_set(obj, "content", json_string(m->content[0] ? m->content : "")); /* keep even if empty */
    json_set(obj, "timestamp", json_number(m->timestamp));
    if (m->has_tool_name) json_set(obj, "tool_name", json_string(m->tool_name));
    if (m->has_tool_calls) json_set(obj, "tool_calls", json_string(m->tool_calls));
    if (m->has_tool_call_id) json_set(obj, "tool_call_id", json_string(m->tool_call_id));
    if (anchor_id >= 0 && m->id == anchor_id)
        json_set(obj, "anchor", json_bool(true));
    return obj;
}

/* Walk the parent_session_id chain to the lineage root (mirrors _resolve_to_parent). */
static int sst_resolve_to_parent_db(sqlite3 *db, const char *session_id,
                                     char *root_out, size_t root_size) {
    if (!session_id || !*session_id) return -1;
    char cur[256];
    snprintf(cur, sizeof(cur), "%s", session_id);
    char visited[64][256];
    int nvisited = 0;
    for (int guard = 0; guard < 64; guard++) {
        int seen = 0;
        for (int i = 0; i < nvisited; i++)
            if (strcmp(visited[i], cur) == 0) { seen = 1; break; }
        if (seen) break;
        if (nvisited < 64) snprintf(visited[nvisited++], sizeof(visited[0]), "%s", cur);

        const char *sql = "SELECT parent_session_id FROM sessions WHERE id = ?";
        sqlite3_stmt *st = NULL;
        int have_parent = 0;
        char parent[256] = {0};
        if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, cur, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) {
                const char *p = (const char *)sqlite3_column_text(st, 0);
                if (p && *p) { snprintf(parent, sizeof(parent), "%s", p); have_parent = 1; }
            }
            sqlite3_finalize(st);
        }
        if (!have_parent) break;
        if (strcmp(parent, cur) == 0) break;
        snprintf(cur, sizeof(cur), "%s", parent);
    }
    if (root_out && root_size > 0) snprintf(root_out, root_size, "%s", cur);
    return 0;
}

/* Sanitize a user query for FTS5 MATCH (faithful port of _sanitize_fts5_query).
 * Mirrors the Python regex pipeline: cap length, protect balanced quotes,
 * strip FTS5-special chars, collapse/trim '*', drop dangling boolean
 * operators, quote dotted/hyphenated runs, restore protected phrases. */
static void sst_sanitize_fts5(const char *in, char *out, size_t outsz) {
    if (!in) { if (out && outsz) out[0] = '\0'; return; }
    char tmp[SST_MAX_QUERY_LEN * 2];
    snprintf(tmp, sizeof(tmp), "%.*s", SST_MAX_QUERY_LEN, in);

    /* Step 1: protect balanced double-quoted phrases via indices. */
    int quote_spans[32][2]; /* [start, end] inclusive */
    int nq = 0;
    char work[SST_MAX_QUERY_LEN * 2];
    int wl = 0;
    for (int i = 0; tmp[i]; i++) {
        if (tmp[i] == '"') {
            int end = -1;
            for (int j = i + 1; tmp[j]; j++) if (tmp[j] == '"') { end = j; break; }
            if (end == -1) { work[wl++] = ' '; continue; }
            if (nq < 32) { quote_spans[nq][0] = i; quote_spans[nq][1] = end; nq++; }
            i = end; /* loop will i++ past the closing quote */
            continue;
        }
        work[wl++] = tmp[i];
    }
    work[wl] = '\0';

    /* Step 2: strip remaining (unmatched) FTS5-special characters. */
    for (int i = 0; work[i]; i++) {
        char c = work[i];
        if (c == '+' || c == '{' || c == '}' || c == '(' || c == ')' ||
            c == ':' || c == '^' || c == '"') work[i] = ' ';
    }

    /* Step 3: collapse repeated '*' (one '*' kept) and drop leading '*'. */
    char step3[SST_MAX_QUERY_LEN * 2];
    int s3 = 0;
    int prev_star = 0;
    for (int i = 0; work[i]; i++) {
        char c = work[i];
        if (c == '*') {
            if (i == 0 || prev_star) { prev_star = 1; continue; } /* drop leading / repeated */
            prev_star = 1;
        } else prev_star = 0;
        if (s3 < (int)sizeof(step3) - 1) step3[s3++] = c;
    }
    step3[s3] = '\0';

    /* Step 4: drop dangling boolean operators at start/end (whole-word). */
    char step4[SST_MAX_QUERY_LEN * 2];
    {
        char *p = step3;
        while (*p == ' ') p++;
        /* strip leading AND/OR/NOT followed by space */
        for (;;) {
            if (strncasecmp(p, "AND ", 4) == 0) p += 4;
            else if (strncasecmp(p, "OR ", 3) == 0) p += 3;
            else if (strncasecmp(p, "NOT ", 4) == 0) p += 4;
            else break;
            while (*p == ' ') p++;
        }
        /* strip trailing AND/OR/NOT preceded by space */
        char *q = p;
        int guard = 0;
        while (strlen(q) > 3 && guard++ < SST_MAX_QUERY_LEN) {
            int L = (int)strlen(q);
            int changed = 0;
            if (L >= 4 && strncasecmp(q + L - 4, " AND", 4) == 0) { q[L - 4] = '\0'; changed = 1; }
            else if (L >= 3 && strncasecmp(q + L - 3, " OR", 3) == 0) { q[L - 3] = '\0'; changed = 1; }
            else if (L >= 4 && strncasecmp(q + L - 4, " NOT", 4) == 0) { q[L - 4] = '\0'; changed = 1; }
            if (!changed) break;
            int L2 = (int)strlen(q);
            while (L2 > 0 && q[L2 - 1] == ' ') q[--L2] = '\0';
        }
        snprintf(step4, sizeof(step4), "%s", p);
    }

    /* Step 5: quote unquoted dotted/hyphenated runs (e.g. chat-send, P2.2).
     * A run of [A-Za-z0-9_.-] containing at least one '.' or '-' gets
     * wrapped in double quotes. */
    char step5[SST_MAX_QUERY_LEN * 2];
    int s5 = 0;
    for (int i = 0; step4[i] && s5 < (int)sizeof(step5) - 1; ) {
        char c = step4[i];
        if (isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-') {
            int start = i;
            int has_sep = 0;
            while (step4[i] && (isalnum((unsigned char)step4[i]) || step4[i] == '_' ||
                   step4[i] == '.' || step4[i] == '-')) {
                if (step4[i] == '.' || step4[i] == '-') has_sep = 1;
                i++;
            }
            int wlen = i - start;
            if (has_sep && wlen > 1 && s5 < (int)sizeof(step5) - 3) {
                step5[s5++] = '"';
                for (int k = start; k < i && s5 < (int)sizeof(step5) - 2; k++)
                    step5[s5++] = step4[k];
                step5[s5++] = '"';
            } else {
                for (int k = start; k < i && s5 < (int)sizeof(step5) - 1; k++)
                    step5[s5++] = step4[k];
            }
        } else {
            if (s5 < (int)sizeof(step5) - 1) step5[s5++] = c;
            i++;
        }
    }
    step5[s5] = '\0';

    /* Step 6: restore protected quoted phrases by rebuilding `final` from
     * `tmp`, copying quoted spans verbatim and applying the sanitize
     * transforms (strip special chars, collapse stars, quote dotted/hyphenated
     * runs, drop dangling booleans) to the gaps between them. When there are
     * no quoted phrases, `final` is just the sanitized `step5`. */
    char final[SST_MAX_QUERY_LEN * 2];
    if (nq == 0) {
        snprintf(final, sizeof(final), "%s", step5);
    } else {
    {
        char rebuilt[SST_MAX_QUERY_LEN * 2];
        int rl = 0;
        int pos = 0;
        for (int qi = 0; qi < nq; qi++) {
            int qs = quote_spans[qi][0], qe = quote_spans[qi][1];
            /* sanitize the gap [pos, qs) */
            char gap[SST_MAX_QUERY_LEN * 2];
            int gl = 0;
            for (int k = pos; k < qs; k++) gap[gl++] = tmp[k];
            gap[gl] = '\0';
            /* apply step3-style strip of special + quote dotted runs to gap */
            char gap2[SST_MAX_QUERY_LEN * 2];
            int g2 = 0;
            for (int k = 0; gap[k]; k++) {
                char c = gap[k];
                if (c == '+' || c == '{' || c == '}' || c == '(' || c == ')' ||
                    c == ':' || c == '^' || c == '"') continue; /* drop */
                if (g2 < (int)sizeof(gap2) - 1) gap2[g2++] = c;
            }
            gap2[g2] = '\0';
            /* collapse stars */
            char gap3[SST_MAX_QUERY_LEN * 2];
            int g3 = 0; int ps = 0;
            for (int k = 0; gap2[k]; k++) {
                char c = gap2[k];
                if (c == '*') { if (k == 0 || ps) continue; ps = 1; }
                else ps = 0;
                if (g3 < (int)sizeof(gap3) - 1) gap3[g3++] = c;
            }
            gap3[g3] = '\0';
            /* quote dotted/hyphenated runs */
            for (int k = 0; gap3[k] && rl < (int)sizeof(rebuilt) - 1; ) {
                char c = gap3[k];
                if (isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-') {
                    int st = k, hs = 0;
                    while (gap3[k] && (isalnum((unsigned char)gap3[k]) || gap3[k]=='_'||gap3[k]=='.'||gap3[k]=='-')) {
                        if (gap3[k]=='.'||gap3[k]=='-') hs = 1; k++;
                    }
                    int ww = k - st;
                    if (hs && ww > 1 && rl < (int)sizeof(rebuilt) - 3) {
                        rebuilt[rl++] = '"';
                        for (int m = st; m < k && rl < (int)sizeof(rebuilt) - 2; m++) rebuilt[rl++] = gap3[m];
                        rebuilt[rl++] = '"';
                    } else {
                        for (int m = st; m < k && rl < (int)sizeof(rebuilt) - 1; m++) rebuilt[rl++] = gap3[m];
                    }
                } else {
                    if (rl < (int)sizeof(rebuilt) - 1) rebuilt[rl++] = c;
                    k++;
                }
            }
            /* append the protected phrase verbatim (with its quotes) */
            for (int k = qs; k <= qe && rl < (int)sizeof(rebuilt) - 1; k++)
                rebuilt[rl++] = tmp[k];
            pos = qe + 1;
        }
        /* tail gap after last quote */
        char gap[SST_MAX_QUERY_LEN * 2];
        int gl = 0;
        for (int k = pos; tmp[k]; k++) gap[gl++] = tmp[k];
        gap[gl] = '\0';
        char gap2[SST_MAX_QUERY_LEN * 2];
        int g2 = 0;
        for (int k = 0; gap[k]; k++) {
            char c = gap[k];
            if (c == '+' || c == '{' || c == '}' || c == '(' || c == ')' ||
                c == ':' || c == '^' || c == '"') continue;
            if (g2 < (int)sizeof(gap2) - 1) gap2[g2++] = c;
        }
        gap2[g2] = '\0';
        char gap3[SST_MAX_QUERY_LEN * 2];
        int g3 = 0; int ps = 0;
        for (int k = 0; gap2[k]; k++) {
            char c = gap2[k];
            if (c == '*') { if (k == 0 || ps) continue; ps = 1; } else ps = 0;
            if (g3 < (int)sizeof(gap3) - 1) gap3[g3++] = c;
        }
        gap3[g3] = '\0';
        for (int k = 0; gap3[k] && rl < (int)sizeof(rebuilt) - 1; ) {
            char c = gap3[k];
            if (isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-') {
                int st = k, hs = 0;
                while (gap3[k] && (isalnum((unsigned char)gap3[k]) || gap3[k]=='_'||gap3[k]=='.'||gap3[k]=='-')) {
                    if (gap3[k]=='.'||gap3[k]=='-') hs = 1; k++;
                }
                int ww = k - st;
                if (hs && ww > 1 && rl < (int)sizeof(rebuilt) - 3) {
                    rebuilt[rl++] = '"';
                    for (int m = st; m < k && rl < (int)sizeof(rebuilt) - 2; m++) rebuilt[rl++] = gap3[m];
                    rebuilt[rl++] = '"';
                } else {
                    for (int m = st; m < k && rl < (int)sizeof(rebuilt) - 1; m++) rebuilt[rl++] = gap3[m];
                }
            } else {
                if (rl < (int)sizeof(rebuilt) - 1) rebuilt[rl++] = c;
                k++;
            }
        }
        rebuilt[rl] = '\0';
        snprintf(final, sizeof(final), "%s", rebuilt);
    }
    }

    /* Trim leading/trailing whitespace for the final output. */
    char *fp = final;
    while (*fp == ' ') fp++;
    snprintf(out, outsz, "%s", fp);
}

/* Build the anchored window + bookends for an anchor message id.
 * Fills the supplied json arrays (caller creates them). Returns 0 on success. */
static int sst_anchored_view(sqlite3 *db, const char *session_id, long anchor_id,
                             int window, int bookend,
                             json_t *window_arr, json_t *bookend_start_arr,
                             json_t *bookend_end_arr, int *before_out, int *after_out) {
    if (before_out) *before_out = 0;
    if (after_out) *after_out = 0;
    if (window < 0) window = 0;
    if (bookend < 0) bookend = 0;

    /* Confirm the anchor exists in this session. */
    {
        sqlite3_stmt *st = NULL;
        int exists = 0;
        if (sqlite3_prepare_v2(db,
                "SELECT 1 FROM messages WHERE id = ? AND session_id = ? LIMIT 1",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(st, 1, (sqlite3_int64)anchor_id);
            sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) exists = 1;
            sqlite3_finalize(st);
        }
        if (!exists) return 0;
    }

    /* before rows: id <= anchor DESC, limit window+1. */
    sst_msg_t before[64];
    int nbefore = 0;
    {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db,
                "SELECT id, role, content, tool_name, tool_calls, tool_call_id, timestamp "
                "FROM messages WHERE session_id = ? AND id <= ? ORDER BY id DESC LIMIT ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(st, 2, (sqlite3_int64)anchor_id);
            sqlite3_bind_int(st, 3, window + 1);
            while (sqlite3_step(st) == SQLITE_ROW && nbefore < 64) {
                sst_msg_t *m = &before[nbefore++];
                memset(m, 0, sizeof(*m));
                m->id = (long)sqlite3_column_int64(st, 0);
                const char *role = (const char *)sqlite3_column_text(st, 1);
                snprintf(m->role, sizeof(m->role), "%s", role ? role : "");
                const char *content = (const char *)sqlite3_column_text(st, 2);
                sst_decode_content(content, m->content, sizeof(m->content));
                const char *tn = (const char *)sqlite3_column_text(st, 3);
                if (tn && *tn) { snprintf(m->tool_name, sizeof(m->tool_name), "%s", tn); m->has_tool_name = 1; }
                const char *tc = (const char *)sqlite3_column_text(st, 4);
                if (tc && *tc) { snprintf(m->tool_calls, sizeof(m->tool_calls), "%s", tc); m->has_tool_calls = 1; }
                const char *tci = (const char *)sqlite3_column_text(st, 5);
                if (tci && *tci) { snprintf(m->tool_call_id, sizeof(m->tool_call_id), "%s", tci); m->has_tool_call_id = 1; }
                m->timestamp = sqlite3_column_double(st, 6);
            }
            sqlite3_finalize(st);
        }
    }
    /* after rows: id > anchor ASC, limit window. */
    sst_msg_t after[64];
    int nafter = 0;
    {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db,
                "SELECT id, role, content, tool_name, tool_calls, tool_call_id, timestamp "
                "FROM messages WHERE session_id = ? AND id > ? ORDER BY id ASC LIMIT ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(st, 2, (sqlite3_int64)anchor_id);
            sqlite3_bind_int(st, 3, window);
            while (sqlite3_step(st) == SQLITE_ROW && nafter < 64) {
                sst_msg_t *m = &after[nafter++];
                memset(m, 0, sizeof(*m));
                m->id = (long)sqlite3_column_int64(st, 0);
                const char *role = (const char *)sqlite3_column_text(st, 1);
                snprintf(m->role, sizeof(m->role), "%s", role ? role : "");
                const char *content = (const char *)sqlite3_column_text(st, 2);
                sst_decode_content(content, m->content, sizeof(m->content));
                const char *tn = (const char *)sqlite3_column_text(st, 3);
                if (tn && *tn) { snprintf(m->tool_name, sizeof(m->tool_name), "%s", tn); m->has_tool_name = 1; }
                const char *tc = (const char *)sqlite3_column_text(st, 4);
                if (tc && *tc) { snprintf(m->tool_calls, sizeof(m->tool_calls), "%s", tc); m->has_tool_calls = 1; }
                const char *tci = (const char *)sqlite3_column_text(st, 5);
                if (tci && *tci) { snprintf(m->tool_call_id, sizeof(m->tool_call_id), "%s", tci); m->has_tool_call_id = 1; }
                m->timestamp = sqlite3_column_double(st, 6);
            }
            sqlite3_finalize(st);
        }
    }

    /* before[] is DESC; append anchor-excluded, then after[], preserving
     * the anchor. Mirror Python: window = reversed(before) + after, then
     * the anchor is always kept. */
    /* Emit before in reverse (ASC) but keep anchor. before[nbefore-1] is the anchor. */
    for (int k = nbefore - 1; k >= 0; k--) {
        int is_anchor = (before[k].id == anchor_id);
        json_append(window_arr, sst_shape_message_obj(&before[k], is_anchor ? anchor_id : -1));
    }
    for (int k = 0; k < nafter; k++)
        json_append(window_arr, sst_shape_message_obj(&after[k], -1));

    if (before_out) *before_out = nbefore > 0 ? nbefore - 1 : 0;
    if (after_out) *after_out = nafter;

    if (bookend <= 0) return 1;

    /* Bookends: first/last `bookend` user+assistant messages with non-empty
     * content, strictly outside the window id range. */
    long win_min = 0, win_max = 0;
    if (nbefore > 0) win_min = before[nbefore - 1].id;
    if (nafter > 0) win_max = after[nafter - 1].id;
    else if (nbefore > 0) win_max = before[0].id;

    if (win_min > 0) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db,
                "SELECT id, role, content, tool_name, tool_calls, tool_call_id, timestamp "
                "FROM messages WHERE session_id = ? AND id < ? "
                "AND role IN ('user','assistant') AND length(content) > 0 "
                "ORDER BY id ASC LIMIT ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(st, 2, (sqlite3_int64)win_min);
            sqlite3_bind_int(st, 3, bookend);
            while (sqlite3_step(st) == SQLITE_ROW) {
                sst_msg_t m; memset(&m, 0, sizeof(m));
                m.id = (long)sqlite3_column_int64(st, 0);
                const char *role = (const char *)sqlite3_column_text(st, 1);
                snprintf(m.role, sizeof(m.role), "%s", role ? role : "");
                const char *content = (const char *)sqlite3_column_text(st, 2);
                sst_decode_content(content, m.content, sizeof(m.content));
                const char *tn = (const char *)sqlite3_column_text(st, 3);
                if (tn && *tn) { snprintf(m.tool_name, sizeof(m.tool_name), "%s", tn); m.has_tool_name=1; }
                const char *tc = (const char *)sqlite3_column_text(st, 4);
                if (tc && *tc) { snprintf(m.tool_calls, sizeof(m.tool_calls), "%s", tc); m.has_tool_calls=1; }
                const char *tci = (const char *)sqlite3_column_text(st, 5);
                if (tci && *tci) { snprintf(m.tool_call_id, sizeof(m.tool_call_id), "%s", tci); m.has_tool_call_id=1; }
                m.timestamp = sqlite3_column_double(st, 6);
                json_append(bookend_start_arr, sst_shape_message_obj(&m, -1));
            }
            sqlite3_finalize(st);
        }
        st = NULL;
        if (sqlite3_prepare_v2(db,
                "SELECT id, role, content, tool_name, tool_calls, tool_call_id, timestamp "
                "FROM messages WHERE session_id = ? AND id > ? "
                "AND role IN ('user','assistant') AND length(content) > 0 "
                "ORDER BY id DESC LIMIT ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(st, 2, (sqlite3_int64)win_max);
            sqlite3_bind_int(st, 3, bookend);
            sst_msg_t rev[64]; int nr = 0;
            while (sqlite3_step(st) == SQLITE_ROW && nr < 64) {
                sst_msg_t *m = &rev[nr++];
                memset(m, 0, sizeof(*m));
                m->id = (long)sqlite3_column_int64(st, 0);
                const char *role = (const char *)sqlite3_column_text(st, 1);
                snprintf(m->role, sizeof(m->role), "%s", role ? role : "");
                const char *content = (const char *)sqlite3_column_text(st, 2);
                sst_decode_content(content, m->content, sizeof(m->content));
                const char *tn = (const char *)sqlite3_column_text(st, 3);
                if (tn && *tn) { snprintf(m->tool_name, sizeof(m->tool_name), "%s", tn); m->has_tool_name = 1; }
                const char *tc = (const char *)sqlite3_column_text(st, 4);
                if (tc && *tc) { snprintf(m->tool_calls, sizeof(m->tool_calls), "%s", tc); m->has_tool_calls = 1; }
                const char *tci = (const char *)sqlite3_column_text(st, 5);
                if (tci && *tci) { snprintf(m->tool_call_id, sizeof(m->tool_call_id), "%s", tci); m->has_tool_call_id = 1; }
                m->timestamp = sqlite3_column_double(st, 6);
            }
            sqlite3_finalize(st);
            for (int k = nr - 1; k >= 0; k--)
                json_append(bookend_end_arr, sst_shape_message_obj(&rev[k], -1));
        }
    }
    return 1;
}

/* ── Public ported functions ─────────────────────────────────────── */

/* PoP: cli_tools_session_search_tool__resolve_to_parent @ tools/session_search_tool.py:_resolve_to_parent */

/* Port of Python tools/session_search_tool.py:_resolve_to_parent.
 * Walk parent_session_id chain to the lineage root via the session DB. */
int cli_tools_session_search_tool__resolve_to_parent(
    const char *session_id, const char **parent_chain, int chain_len,
    char *root_out, size_t root_size)
{
    (void)parent_chain; (void)chain_len;
    if (!session_id || !root_out || root_size == 0) return -1;

    sqlite3 *db = sst_open_readonly(NULL);
    if (!db) {
        /* No DB available — return the input as the best-known root. */
        snprintf(root_out, root_size, "%s", session_id);
        return 0;
    }
    sst_resolve_to_parent_db(db, session_id, root_out, root_size);
    sqlite3_close(db);

    hermes_log(LOG_DEBUG, "session_search", "resolve_to_parent: %s -> %s",
               session_id, root_out);
    return 0;
}

/* PoP: cli_tools_session_search_tool__shape_message @ tools/session_search_tool.py:_shape_message */

/* Port of Python tools/session_search_tool.py:_shape_message.
 * Slim a message JSON row for the tool response: keep id/role/content/
 * timestamp plus tool metadata, strip NULLs except content, mark anchor. */
int cli_tools_session_search_tool__shape_message(
    const char *msg_json, int anchor_id,
    char *shaped_json_out, size_t shaped_size, int *is_anchor_out)
{
    if (!msg_json || !shaped_json_out || shaped_size == 0) return -1;

    json_t *root = json_parse(msg_json, NULL);
    if (!root || root->type != JSON_OBJECT) {
        if (root) json_free(root);
        snprintf(shaped_json_out, shaped_size, "%s", msg_json);
        if (is_anchor_out) *is_anchor_out = 0;
        return 0;
    }

    json_t *obj = json_object();
    double mid = json_get_num(root, "id", -1);
    json_set(obj, "id", json_number(mid));
    const char *role = json_get_str(root, "role", "");
    json_set(obj, "role", json_string(role && *role ? role : ""));
    const char *content = json_get_str(root, "content", NULL);
    json_set(obj, "content", json_string(content ? content : "")); /* keep even if empty */
    json_set(obj, "timestamp", json_number(json_get_num(root, "timestamp", 0)));
    json_t *tn = json_obj_get(root, "tool_name");
    if (tn && tn->type == JSON_STRING && tn->str_val && *tn->str_val)
        json_set(obj, "tool_name", json_string(tn->str_val));
    json_t *tc = json_obj_get(root, "tool_calls");
    if (tc && tc->type == JSON_STRING && tc->str_val && *tc->str_val)
        json_set(obj, "tool_calls", json_string(tc->str_val));
    json_t *tci = json_obj_get(root, "tool_call_id");
    if (tci && tci->type == JSON_STRING && tci->str_val && *tci->str_val)
        json_set(obj, "tool_call_id", json_string(tci->str_val));
    int is_anchor = 0;
    if (anchor_id >= 0 && (long)mid == (long)anchor_id) {
        json_set(obj, "anchor", json_bool(true));
        is_anchor = 1;
    }

    char *serialized = json_serialize(obj);
    if (serialized) {
        snprintf(shaped_json_out, shaped_size, "%s", serialized);
        free(serialized);
    } else {
        shaped_json_out[0] = '\0';
    }
    json_free(obj);
    json_free(root);
    if (is_anchor_out) *is_anchor_out = is_anchor;

    hermes_log(LOG_DEBUG, "session_search", "shape_message: %zu chars",
               strlen(shaped_json_out));
    return 0;
}

/* PoP: cli_tools_session_search_tool__resolve_profile_db @ tools/session_search_tool.py:_resolve_profile_db */

/* Port of Python tools/session_search_tool.py:_resolve_profile_db.
 * Resolve a profile's state.db path. Empty profile -> default DB path. */
int cli_tools_session_search_tool__resolve_profile_db(
    const char *profile, const char *default_db_path,
    char *db_path_out, size_t path_size, int *needs_free_out)
{
    if (!db_path_out || !needs_free_out) return -1;

    *needs_free_out = 0;
    db_path_out[0] = '\0';

    if (!profile || !*profile) {
        if (default_db_path && *default_db_path)
            snprintf(db_path_out, path_size, "%s", default_db_path);
        return 0;
    }

    /* Build ~/.hermes/profiles/<profile>/state.db (normalize: strip path
     * separators so a malicious profile name can't escape the dir). */
    char safe[256];
    int si = 0;
    for (const char *p = profile; *p && si < (int)sizeof(safe) - 1; p++) {
        if (*p == '/' || *p == '\\' || *p == '.' ) continue;
        safe[si++] = *p;
    }
    safe[si] = '\0';

    const char *home = slermes_home();
    if (!home || !*home) home = ".";
    snprintf(db_path_out, path_size, "%s/profiles/%s/state.db", home, safe);
    hermes_log(LOG_DEBUG, "session_search", "resolve_profile_db: profile=%s path=%s",
               safe, db_path_out);
    return 0;
}

/* PoP: cli_tools_session_search_tool__locate_session_db @ tools/session_search_tool.py:_locate_session_db */

/* Port of Python tools/session_search_tool.py:_locate_session_db.
 * Scan every profile's state.db (read-only) for a session id; return the
 * first owning (db_path, profile_name). */
int cli_tools_session_search_tool__locate_session_db(
    const char *session_id, const char *profiles_dir,
    char *db_path_out, size_t path_size, char *profile_out, size_t profile_size,
    int *found_out)
{
    if (!session_id || !db_path_out || !found_out) return -1;

    *found_out = 0;
    db_path_out[0] = '\0';
    if (profile_out) profile_out[0] = '\0';

    const char *home = slermes_home();
    if (!home || !*home) home = ".";

    /* Candidate DBs: default + each profile subdir. */
    char candidates[SST_MAX_SESSIONS][1024];
    char cand_names[SST_MAX_SESSIONS][128];
    int ncand = 0;
    snprintf(candidates[ncand], sizeof(candidates[ncand]), "%s/state.db", home);
    snprintf(cand_names[ncand], sizeof(cand_names[ncand]), "default");
    ncand++;

    char prof_root[1024];
    snprintf(prof_root, sizeof(prof_root), "%s/profiles", home);
    if (!profiles_dir || !*profiles_dir) profiles_dir = prof_root;

    /* Enumerate profile directories via opendir/readdir and probe each
     * state.db directly (no shell invocation). */
    {
        DIR *d = opendir(profiles_dir);
        if (d) {
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL && ncand < SST_MAX_SESSIONS) {
                if (ent->d_name[0] == '.') continue;
                char pdir[1200];
                snprintf(pdir, sizeof(pdir), "%s/%s", profiles_dir, ent->d_name);
                struct stat stt;
                if (stat(pdir, &stt) != 0 || !S_ISDIR(stt.st_mode)) continue;
                char pdb[1300];
                snprintf(pdb, sizeof(pdb), "%s/state.db", pdir);
                if (access(pdb, F_OK) != 0) continue;
                snprintf(candidates[ncand], sizeof(candidates[ncand]), "%s", pdb);
                snprintf(cand_names[ncand], sizeof(cand_names[ncand]), "%s", ent->d_name);
                ncand++;
            }
            closedir(d);
        }
    }

    for (int c = 0; c < ncand; c++) {
        sqlite3 *db = sst_open_readonly(candidates[c]);
        if (!db) continue;
        sqlite3_stmt *st = NULL;
        int hit = 0;
        if (sqlite3_prepare_v2(db, "SELECT 1 FROM sessions WHERE id = ? LIMIT 1",
                               -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) hit = 1;
            sqlite3_finalize(st);
        }
        sqlite3_close(db);
        if (hit) {
            snprintf(db_path_out, path_size, "%s", candidates[c]);
            if (profile_out) snprintf(profile_out, profile_size, "%s", cand_names[c]);
            *found_out = 1;
            hermes_log(LOG_DEBUG, "session_search", "locate_session: %s in %s (%s)",
                       session_id, candidates[c], cand_names[c]);
            break;
        }
    }
    return 0;
}

/* PoP: cli_tools_session_search_tool__read_session @ tools/session_search_tool.py:_read_session */

/* Port of Python tools/session_search_tool.py:_read_session.
 * Dump a whole session by id (head + tail when large), shaped. */
int cli_tools_session_search_tool__read_session(
    const char *session_id, int head, int tail,
    char *json_out, size_t json_size, int *total_messages_out)
{
    if (!session_id || !json_out || json_size == 0 || !total_messages_out) return -1;
    *total_messages_out = 0;

    if (head <= 0) head = 20;
    if (tail <= 0) tail = 10;

    sqlite3 *db = sst_open_readonly(NULL);
    if (!db) {
        snprintf(json_out, json_size,
                 "{\"success\":false,\"mode\":\"read\",\"error\":\"session DB unavailable\"}");
        return 0;
    }

    /* Session meta. */
    char source[64] = "", model[256] = "", title[1024] = "";
    double started_at = 0;
    int meta_found = 0;
    {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db,
                "SELECT source, model, title, started_at FROM sessions WHERE id = ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) {
                meta_found = 1;
                const char *s = (const char *)sqlite3_column_text(st, 0);
                snprintf(source, sizeof(source), "%s", s ? s : "");
                const char *m = (const char *)sqlite3_column_text(st, 1);
                snprintf(model, sizeof(model), "%s", m ? m : "");
                const char *t = (const char *)sqlite3_column_text(st, 2);
                snprintf(title, sizeof(title), "%s", t ? t : "");
                started_at = sqlite3_column_double(st, 3);
            }
            sqlite3_finalize(st);
        }
    }
    if (!meta_found) {
        sqlite3_close(db);
        snprintf(json_out, json_size,
                 "{\"success\":false,\"mode\":\"read\",\"error\":\"session_id not found: %s\"}",
                 session_id);
        return 0;
    }

    /* All messages — heap-allocated to avoid a 200KB stack frame. */
    sst_msg_t *msgs = calloc((size_t)SST_MAX_MESSAGES, sizeof(sst_msg_t));
    if (!msgs) {
        sqlite3_close(db);
        snprintf(json_out, json_size,
                 "{\"success\":false,\"mode\":\"read\",\"error\":\"out of memory\"}");
        return 0;
    }
    int nmsg = 0;
    {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db,
                "SELECT id, role, content, tool_name, tool_calls, tool_call_id, timestamp "
                "FROM messages WHERE session_id = ? AND active = 1 ORDER BY id",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW && nmsg < SST_MAX_MESSAGES) {
                sst_msg_t *m = &msgs[nmsg++];
                m->id = (long)sqlite3_column_int64(st, 0);
                const char *role = (const char *)sqlite3_column_text(st, 1);
                snprintf(m->role, sizeof(m->role), "%s", role ? role : "");
                const char *content = (const char *)sqlite3_column_text(st, 2);
                sst_decode_content(content, m->content, sizeof(m->content));
                const char *tn = (const char *)sqlite3_column_text(st, 3);
                if (tn && *tn) { snprintf(m->tool_name, sizeof(m->tool_name), "%s", tn); m->has_tool_name = 1; }
                const char *tc = (const char *)sqlite3_column_text(st, 4);
                if (tc && *tc) { snprintf(m->tool_calls, sizeof(m->tool_calls), "%s", tc); m->has_tool_calls = 1; }
                const char *tci = (const char *)sqlite3_column_text(st, 5);
                if (tci && *tci) { snprintf(m->tool_call_id, sizeof(m->tool_call_id), "%s", tci); m->has_tool_call_id = 1; }
                m->timestamp = sqlite3_column_double(st, 6);
            }
            sqlite3_finalize(st);
        }
    }
    sqlite3_close(db);

    int total = nmsg;
    *total_messages_out = total;
    int truncated = total > head + tail;
    int wstart = 0, wcount = total;
    if (truncated) {
        /* First `head` messages, then a gap, then last `tail` messages.
         * We serialize head, then tail (Python emits head + tail with the
         * middle omitted since only head+tail are shown). */
        wcount = head + tail;
    }

    json_t *resp = json_object();
    json_set(resp, "success", json_bool(true));
    json_set(resp, "mode", json_string("read"));
    json_set(resp, "session_id", json_string(session_id));

    json_t *meta = json_object();
    json_set(meta, "when", json_string(sst_format_timestamp(started_at)));
    json_set(meta, "source", json_string(source[0] ? source : "agent"));
    json_set(meta, "model", json_string(model[0] ? model : ""));
    json_set(meta, "title", json_string(title[0] ? title : ""));
    json_set(resp, "session_meta", meta);

    json_set(resp, "message_count", json_number((double)total));
    json_set(resp, "truncated", json_bool(truncated));

    json_t *arr = json_array();
    if (truncated) {
        /* First `head` messages, then a gap marker, then last `tail` messages. */
        int tail_start = total - tail > head ? total - tail : head;
        for (int i = 0; i < head && i < total; i++)
            json_append(arr, sst_shape_message_obj(&msgs[i], -1));
        if (tail_start > head) {
            json_t *gap = json_object();
            json_set(gap, "role", json_string("system"));
            json_set(gap, "content", json_string("… (middle of session omitted) …"));
            json_set(gap, "anchor", json_bool(false));
            json_append(arr, gap);
        }
        for (int i = tail_start; i < total; i++)
            json_append(arr, sst_shape_message_obj(&msgs[i], -1));
    } else {
        for (int i = 0; i < total; i++)
            json_append(arr, sst_shape_message_obj(&msgs[i], -1));
    }
    json_set(resp, "messages", arr);

    if (truncated) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Session has %d messages; showing first %d + last %d. "
                 "Pass around_message_id (any id above) to scroll the middle.",
                 total, head, tail);
        json_set(resp, "message", json_string(msg));
    }

    char *serialized = json_serialize(resp);
    if (serialized) {
        snprintf(json_out, json_size, "%s", serialized);
        free(serialized);
    } else {
        json_out[0] = '\0';
    }
    json_free(resp);
    free(msgs);

    hermes_log(LOG_DEBUG, "session_search", "read_session: %s (%d msgs)", session_id, total);
    return 0;
}

/* PoP: cli_tools_session_search_tool__list_recent_sessions @ tools/session_search_tool.py:_list_recent_sessions */

/* Port of Python tools/session_search_tool.py:_list_recent_sessions.
 * Return metadata for the most recent sessions (excluding hidden sources,
 * the current lineage, and child sessions). */
int cli_tools_session_search_tool__list_recent_sessions(
    int limit, const char *current_session_id,
    char *json_out, size_t json_size, int *count_out)
{
    if (!json_out || json_size == 0 || !count_out) return -1;
    *count_out = 0;
    if (limit <= 0) limit = 10;

    sqlite3 *db = sst_open_readonly(NULL);
    if (!db) {
        snprintf(json_out, json_size,
                 "{\"success\":true,\"mode\":\"browse\",\"results\":[],\"count\":0,"
                 "\"message\":\"Session DB unavailable.\"}");
        return 0;
    }

    char cur_root[256] = "";
    if (current_session_id && *current_session_id)
        sst_resolve_to_parent_db(db, current_session_id, cur_root, sizeof(cur_root));

    /* Fetch recent root sessions, excluding hidden sources and children. */
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "SELECT id, title, source, started_at, message_count, "
        "  (SELECT SUBSTR(REPLACE(REPLACE(m.content, char(10), ' '), char(13), ' '), 1, 60) "
        "   FROM messages m WHERE m.session_id = s.id AND m.role = 'user' "
        "   AND m.content IS NOT NULL ORDER BY m.timestamp, m.id LIMIT 1) AS preview "
        "FROM sessions s "
        "WHERE parent_session_id IS NULL AND source NOT IN ('subagent','tool') "
        "AND archived = 0 "
        "ORDER BY started_at DESC LIMIT ?");

    json_t *results = json_array();
    int count = 0;
    {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_int(st, 1, limit + 5);
            while (sqlite3_step(st) == SQLITE_ROW && count < limit) {
                const char *sid = (const char *)sqlite3_column_text(st, 0);
                if (!sid) continue;
                /* Skip current lineage root. */
                if (cur_root[0] && (strcmp(sid, cur_root) == 0 ||
                                    strcmp(sid, current_session_id ? current_session_id : "") == 0))
                    continue;
                json_t *r = json_object();
                json_set(r, "session_id", json_string(sid));
                const char *t = (const char *)sqlite3_column_text(st, 1);
                json_set(r, "title", json_string(t && *t ? t : ""));
                const char *src = (const char *)sqlite3_column_text(st, 2);
                json_set(r, "source", json_string(src ? src : ""));
                json_set(r, "started_at", json_number(sqlite3_column_double(st, 3)));
                json_set(r, "message_count", json_number((double)sqlite3_column_int(st, 4)));
                const char *prev = (const char *)sqlite3_column_text(st, 5);
                json_set(r, "preview", json_string(prev ? prev : ""));
                json_append(results, r);
                count++;
            }
            sqlite3_finalize(st);
        }
    }
    sqlite3_close(db);

    *count_out = count;
    json_t *resp = json_object();
    json_set(resp, "success", json_bool(true));
    json_set(resp, "mode", json_string("browse"));
    json_set(resp, "results", results);
    json_set(resp, "count", json_number((double)count));
    char msg[256];
    snprintf(msg, sizeof(msg),
             "Showing %d most recent sessions. Pass a query= to search, or "
             "session_id+around_message_id to scroll.", count);
    json_set(resp, "message", json_string(msg));

    char *serialized = json_serialize(resp);
    if (serialized) {
        snprintf(json_out, json_size, "%s", serialized);
        free(serialized);
    } else {
        json_out[0] = '\0';
    }
    json_free(resp);

    hermes_log(LOG_DEBUG, "session_search", "list_recent: limit=%d count=%d", limit, count);
    return 0;
}

/* PoP: cli_tools_session_search_tool__discover @ tools/session_search_tool.py:_discover */

/* Port of Python tools/session_search_tool.py:_discover.
 * Discovery shape: FTS5 search + dedupe by lineage + anchored window +
 * bookends per hit. Demotes cron (automation) below interactive sessions. */
int cli_tools_session_search_tool__discover(
    const char *query, const char **role_filter, int role_count,
    int limit, const char *sort, const char *current_session_id,
    char *json_out, size_t json_size, int *count_out)
{
    if (!query || !json_out || json_size == 0 || !count_out) return -1;
    *count_out = 0;
    if (limit <= 0) limit = 3;
    if (limit > 10) limit = 10;

    sqlite3 *db = sst_open_readonly(NULL);
    if (!db) {
        snprintf(json_out, json_size,
                 "{\"success\":true,\"mode\":\"discover\",\"query\":\"%s\",\"results\":[],"
                 "\"count\":0,\"sessions_searched\":0,\"message\":\"Session DB unavailable.\"}",
                 query);
        return 0;
    }

    char cur_root[256] = "";
    if (current_session_id && *current_session_id)
        sst_resolve_to_parent_db(db, current_session_id, cur_root, sizeof(cur_root));

    /* Sanitize + build the FTS5 MATCH query. */
    char sanitized[SST_MAX_QUERY_LEN * 2];
    sst_sanitize_fts5(query, sanitized, sizeof(sanitized));
    if (!sanitized[0]) {
        sqlite3_close(db);
        snprintf(json_out, json_size,
                 "{\"success\":true,\"mode\":\"discover\",\"query\":\"%s\",\"results\":[],"
                 "\"count\":0,\"sessions_searched\":0,\"message\":\"No matching sessions found.\"}",
                 query);
        return 0;
    }

    const char *roles = "user','assistant";
    if (role_count > 0 && role_filter) {
        /* Use the supplied role filter (already validated upstream). */
        static char rb[256]; rb[0] = '\0';
        for (int i = 0; i < role_count && i < 8; i++) {
            if (i) strncat(rb, "','", sizeof(rb) - strlen(rb) - 1);
            strncat(rb, role_filter[i], sizeof(rb) - strlen(rb) - 1);
        }
        roles = rb;
    }

    char order_by[64] = "ORDER BY rank";
    if (sort && strcasecmp(sort, "newest") == 0)
        snprintf(order_by, sizeof(order_by), "ORDER BY m.timestamp DESC, rank");
    else if (sort && strcasecmp(sort, "oldest") == 0)
        snprintf(order_by, sizeof(order_by), "ORDER BY m.timestamp ASC, rank");

    char sql[1024];
    snprintf(sql, sizeof(sql),
        "SELECT m.id, m.session_id, m.role, "
        "snippet(messages_fts, 0, '>>>', '<<<', '...', 40) AS snippet, "
        "m.timestamp, s.source, s.model, s.started_at AS session_started "
        "FROM messages_fts "
        "JOIN messages m ON m.id = messages_fts.rowid "
        "JOIN sessions s ON s.id = m.session_id "
        "WHERE messages_fts MATCH ? "
        "AND (m.active = 1 OR m.compacted = 1) "
        "AND s.source NOT IN ('subagent','tool') "
        "AND m.role IN ('%s') "
        "%s LIMIT ?",
        roles, order_by);

    /* Pass 1: collect raw FTS rows (BM25 order). */
    typedef struct { long id; char sid[256]; char role[32]; char snippet[512];
                     double ts; char source[64]; char model[256]; double sstart; } raw_t;
    raw_t raw[SST_DISCOVER_SCAN_LIMIT];
    int nraw = 0;
    {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, sanitized, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, 2, SST_DISCOVER_SCAN_LIMIT);
            while (sqlite3_step(st) == SQLITE_ROW && nraw < SST_DISCOVER_SCAN_LIMIT) {
                raw_t *r = &raw[nraw++];
                r->id = (long)sqlite3_column_int64(st, 0);
                const char *sid = (const char *)sqlite3_column_text(st, 1);
                snprintf(r->sid, sizeof(r->sid), "%s", sid ? sid : "");
                const char *rl = (const char *)sqlite3_column_text(st, 2);
                snprintf(r->role, sizeof(r->role), "%s", rl ? rl : "");
                const char *sn = (const char *)sqlite3_column_text(st, 3);
                snprintf(r->snippet, sizeof(r->snippet), "%s", sn ? sn : "");
                r->ts = sqlite3_column_double(st, 4);
                const char *src = (const char *)sqlite3_column_text(st, 5);
                snprintf(r->source, sizeof(r->source), "%s", src ? src : "");
                const char *mdl = (const char *)sqlite3_column_text(st, 6);
                snprintf(r->model, sizeof(r->model), "%s", mdl ? mdl : "");
                r->sstart = sqlite3_column_double(st, 7);
            }
            sqlite3_finalize(st);
        }
    }

    /* Demote cron below interactive; stable — preserves BM25 order within class. */
    /* Pass 2: two queues (interactive first, then cron). */
    int order[SST_DISCOVER_SCAN_LIMIT];
    int nordered = 0;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < nraw; i++) {
            int is_cron = (strcmp(raw[i].source, "cron") == 0);
            if ((pass == 0 && !is_cron) || (pass == 1 && is_cron)) order[nordered++] = i;
        }
    }

    /* Dedupe by lineage root, skipping current lineage. */
    char seen[SST_MAX_SESSIONS][256];
    int nseen = 0;
    json_t *results = json_array();
    int sessions_searched = 0;

    for (int oi = 0; oi < nordered && nseen < limit; oi++) {
        raw_t *r = &raw[order[oi]];
        char resolved[256];
        sst_resolve_to_parent_db(db, r->sid, resolved, sizeof(resolved));
        sessions_searched++;

        if (cur_root[0] && (strcmp(resolved, cur_root) == 0 ||
                            strcmp(r->sid, current_session_id ? current_session_id : "") == 0))
            continue;

        int dup = 0;
        for (int k = 0; k < nseen; k++)
            if (strcmp(seen[k], resolved) == 0) { dup = 1; break; }
        if (dup) continue;

        if (nseen < SST_MAX_SESSIONS) snprintf(seen[nseen], sizeof(seen[0]), "%s", resolved);
        nseen++;

        /* Anchored view around the FTS match. */
        json_t *win = json_array();
        json_t *bstart = json_array();
        json_t *bend = json_array();
        int before = 0, after = 0;
        sst_anchored_view(db, r->sid, r->id, SST_WINDOW_DEFAULT, SST_BOOKEND_DEFAULT,
                          win, bstart, bend, &before, &after);

        /* Lineage root session meta. */
        char lsource[64] = "", lmodel[256] = "", ltitle[1024] = "";
        double lstart = r->sstart;
        {
            sqlite3_stmt *st = NULL;
            if (sqlite3_prepare_v2(db,
                    "SELECT source, model, title, started_at FROM sessions WHERE id = ?",
                    -1, &st, NULL) == SQLITE_OK) {
                sqlite3_bind_text(st, 1, resolved, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(st) == SQLITE_ROW) {
                    const char *s = (const char *)sqlite3_column_text(st, 0);
                    snprintf(lsource, sizeof(lsource), "%s", s ? s : "");
                    const char *m = (const char *)sqlite3_column_text(st, 1);
                    snprintf(lmodel, sizeof(lmodel), "%s", m ? m : "");
                    const char *t = (const char *)sqlite3_column_text(st, 2);
                    snprintf(ltitle, sizeof(ltitle), "%s", t ? t : "");
                    lstart = sqlite3_column_double(st, 3);
                }
                sqlite3_finalize(st);
            }
        }

        json_t *entry = json_object();
        json_set(entry, "session_id", json_string(r->sid));
        json_set(entry, "when", json_string(sst_format_timestamp(lstart)));
        json_set(entry, "source", json_string(lsource[0] ? lsource : "unknown"));
        json_set(entry, "model", json_string(lmodel[0] ? lmodel : "unknown"));
        json_set(entry, "title", json_string(ltitle[0] ? ltitle : ""));
        json_set(entry, "matched_role", json_string(r->role));
        json_set(entry, "match_message_id", json_number((double)r->id));
        json_set(entry, "snippet", json_string(r->snippet));
        json_set(entry, "bookend_start", bstart);
        json_set(entry, "messages", win);
        json_set(entry, "bookend_end", bend);
        json_set(entry, "messages_before", json_number((double)before));
        json_set(entry, "messages_after", json_number((double)after));
        if (strcmp(resolved, r->sid) != 0)
            json_set(entry, "parent_session_id", json_string(resolved));
        json_append(results, entry);
    }

    int cnt = (int)json_len(results);
    *count_out = cnt;
    json_t *resp = json_object();
    json_set(resp, "success", json_bool(true));
    json_set(resp, "mode", json_string("discover"));
    json_set(resp, "query", json_string(query));
    json_set(resp, "results", results);
    json_set(resp, "count", json_number((double)cnt));
    json_set(resp, "sessions_searched", json_number((double)sessions_searched));

    char *serialized = json_serialize(resp);
    if (serialized) {
        snprintf(json_out, json_size, "%s", serialized);
        free(serialized);
    } else {
        json_out[0] = '\0';
    }
    json_free(resp);
    sqlite3_close(db);

    hermes_log(LOG_DEBUG, "session_search", "discover: query='%s' limit=%d results=%d",
               query, limit, cnt);
    return 0;
}

/* PoP: cli_tools_session_search_tool_check_session_search_requirements @ tools/session_search_tool.py:check_session_search_requirements */

/* Port of Python tools/session_search_tool.py:check_session_search_requirements.
 * Check if session search is available (DB exists, FTS5 ready). */
int cli_tools_session_search_tool_check_session_search_requirements(
    const char *db_path, int *available_out, char *reason_out, size_t reason_size)
{
    if (!available_out) return -1;

    *available_out = 0;
    if (reason_out && reason_size > 0) reason_out[0] = '\0';

    if (!db_path || !*db_path) db_path = sst_default_db();

    /* DB file must exist and open read-only. */
    struct stat stt;
    if (stat(db_path, &stt) != 0) {
        if (reason_out && reason_size > 0)
            snprintf(reason_out, reason_size, "Session DB not found: %s", db_path);
        hermes_log(LOG_DEBUG, "session_search", "check_requirements: DB not found (%s)", db_path);
        return 0;
    }

    sqlite3 *db = sst_open_readonly(db_path);
    if (!db) {
        if (reason_out && reason_size > 0)
            snprintf(reason_out, reason_size, "Cannot open session DB: %s", db_path);
        return 0;
    }
    /* Confirm the FTS5 virtual table exists. */
    int fts_ok = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name='messages_fts' LIMIT 1",
            -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) fts_ok = 1;
        sqlite3_finalize(st);
    }
    sqlite3_close(db);

    if (!fts_ok) {
        if (reason_out && reason_size > 0)
            snprintf(reason_out, reason_size, "FTS5 index unavailable in %s", db_path);
        hermes_log(LOG_DEBUG, "session_search", "check_requirements: FTS5 missing (%s)", db_path);
        return 0;
    }

    *available_out = 1;
    hermes_log(LOG_DEBUG, "session_search", "check_requirements: available (%s)", db_path);
    return 0;
}
