/*
 * session_search_tool.c — session-search-tool DB/FTS helpers.
 *
 * Split from port_tools_session_search_tool.c (soul.md: self-contained
 * modules). Message decode/fetch, parent-DB resolution, FTS5 sanitisation,
 * and the anchored-window view. No file-scope state.
 */
#include "session_search_tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>

#define SST_CONTENT_JSON_PREFIX "\x00json:"

void sst_decode_content(const char *raw, char *out, size_t outsz) {
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

int sst_fetch_message(sqlite3 *db, long msg_id, sst_msg_t *m) {
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

int sst_resolve_to_parent_db(sqlite3 *db, const char *session_id,
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

void sst_sanitize_fts5(const char *in, char *out, size_t outsz) {
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

json_t *sst_shape_message_obj(const sst_msg_t *m, long anchor_id) {
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

int sst_anchored_view(sqlite3 *db, const char *session_id, long anchor_id,
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
