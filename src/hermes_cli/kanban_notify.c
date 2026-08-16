/*
 * kanban_notify.c — notification subscriptions for hermes_cli/kanban_db.py
 *
 * Concern: the gateway kanban-notifier's per-task subscription cursor
 * (add/remove/advance/rewind) plus the read/claim helpers
 * (list_notify_subs / unseen_events_for_sub / claim_unseen_events_for_sub).
 * Reuses the write-txn primitives. Self-contained.
 *
 * Minimal includes. No god header.
 *
 * PoP: exact port. Semantic source of truth = hermes_cli/kanban_db.py
 *      (add_notify_sub / remove_notify_sub / advance_notify_cursor /
 *       rewind_notify_cursor / list_notify_subs / unseen_events_for_sub /
 *       claim_unseen_events_for_sub).
 */

#include "kanban_db.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

/* PoP: kdb_add_notify_sub @ hermes_cli/kanban_db.py:add_notify_sub */
int kdb_add_notify_sub(sqlite3 *conn, const char *task_id, const char *platform,
                          const char *chat_id, const char *thread_id,
                          const char *user_id, const char *notifier_profile)
{
    if (!conn || !task_id || !platform || !chat_id) return 0;
    long now = kdb_now();
    if (kdb_write_begin(conn) != 0) return 0;
    int ok = 0;
    sqlite3_stmt *st = NULL;
    /* Port of Python add_notify_sub: new subscriptions start "caught up" —
     * last_event_id snaps to MAX(task_events.id) for the task, NOT the
     * schema default 0. A cursor of 0 would make the notifier replay all
     * historical terminal events on its next tick. */
    if (sqlite3_prepare_v2(conn,
            "INSERT OR IGNORE INTO kanban_notify_subs "
            "(task_id, platform, chat_id, thread_id, user_id, notifier_profile, "
            " created_at, last_event_id) "
            "VALUES (?,?,?,?,?,?,?,"
            "COALESCE((SELECT MAX(id) FROM task_events WHERE task_id = ?), 0))",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, user_id ? user_id : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 6, notifier_profile ? notifier_profile : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 7, now);
        sqlite3_bind_text(st, 8, task_id, -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(st) == SQLITE_DONE);
        sqlite3_finalize(st);
    }
    if (ok && notifier_profile) {
        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE kanban_notify_subs SET notifier_profile=? "
                "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=? "
                "AND (notifier_profile IS NULL OR notifier_profile='')", -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_text(up, 1, notifier_profile, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 2, task_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 3, platform, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 4, chat_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 5, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
            sqlite3_step(up); sqlite3_finalize(up);
        }
    }
    kdb_write_end(conn, 1);
    return ok;
}

/* PoP: kdb_remove_notify_sub @ hermes_cli/kanban_db.py:remove_notify_sub */
int kdb_remove_notify_sub(sqlite3 *conn, const char *task_id, const char *platform,
                             const char *chat_id, const char *thread_id)
{
    if (!conn || !task_id || !platform || !chat_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn,
            "DELETE FROM kanban_notify_subs WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(conn) > 0);
        sqlite3_finalize(st);
    }
    kdb_write_end(conn, 1);
    return ok;
}

/* PoP: kdb_advance_notify_cursor @ hermes_cli/kanban_db.py:advance_notify_cursor */
int kdb_advance_notify_cursor(sqlite3 *conn, const char *task_id, const char *platform,
                                 const char *chat_id, const char *thread_id, int new_cursor)
{
    if (!conn || !task_id || !platform || !chat_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn,
            "UPDATE kanban_notify_subs SET last_event_id=? "
            "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, new_cursor);
        sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(st) == SQLITE_DONE);
        sqlite3_finalize(st);
    }
    kdb_write_end(conn, 1);
    return ok;
}

/* PoP: kdb_rewind_notify_cursor @ hermes_cli/kanban_db.py:rewind_notify_cursor */
int kdb_rewind_notify_cursor(sqlite3 *conn, const char *task_id, const char *platform,
                                const char *chat_id, const char *thread_id,
                                int claimed_cursor, int old_cursor)
{
    if (!conn || !task_id || !platform || !chat_id) return 0;
    if (kdb_write_begin(conn) != 0) return 0;
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(conn,
            "UPDATE kanban_notify_subs SET last_event_id=? "
            "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=? AND last_event_id=?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, old_cursor);
        sqlite3_bind_text(st, 2, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 6, claimed_cursor);
        ok = (sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(conn) > 0);
        sqlite3_finalize(st);
    }
    kdb_write_end(conn, 1);
    return ok;
}

/* PoP: kdb_list_notify_subs @ hermes_cli/kanban_db.py:list_notify_subs */
char **kdb_list_notify_subs(sqlite3 *conn, const char *task_id, int *out_n)
{
    if (!conn) { if (out_n) *out_n = 0; return NULL; }
    sqlite3_stmt *st = NULL;
    int n = 0, cap = 8;
    char **rows = malloc(sizeof(char*) * cap);
    const char *q = task_id
        ? "SELECT * FROM kanban_notify_subs WHERE task_id=?"
        : "SELECT * FROM kanban_notify_subs";
    if (sqlite3_prepare_v2(conn, q, -1, &st, NULL) == SQLITE_OK) {
        if (task_id) sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *tid = (const char*)sqlite3_column_text(st, 0);
            const char *plat = (const char*)sqlite3_column_text(st, 1);
            const char *cid = (const char*)sqlite3_column_text(st, 2);
            const char *thr = (const char*)sqlite3_column_text(st, 3);
            int leid = sqlite3_column_type(st, 8) == SQLITE_NULL ? 0 : (int)sqlite3_column_int64(st, 8);
            char buf[1024];
            snprintf(buf, sizeof(buf),
                     "{\"task_id\":%s,\"platform\":%s,\"chat_id\":%s,\"thread_id\":%s,"
                     "\"last_event_id\":%d}",
                     tid ? tid : "", plat ? plat : "", cid ? cid : "", thr ? thr : "", leid);
            if (n >= cap) { cap *= 2; rows = realloc(rows, sizeof(char*) * cap); }
            rows[n++] = strdup(buf);
        }
        sqlite3_finalize(st);
    }
    rows = realloc(rows, sizeof(char*) * (n + 1));
    rows[n] = NULL;
    if (out_n) *out_n = n;
    return rows;
}

/* PoP: kdb_unseen_events_for_sub @ hermes_cli/kanban_db.py:unseen_events_for_sub */
int kdb_unseen_events_for_sub(sqlite3 *conn, const char *task_id,
                               const char *platform, const char *chat_id,
                               const char *thread_id, char **kinds, int n_kinds,
                               int *out_new_cursor, kanban_event_t ***out_events, int *out_n)
{
    if (!conn || !task_id || !platform || !chat_id) {
        if (out_new_cursor) *out_new_cursor = 0;
        if (out_events) *out_events = NULL;
        if (out_n) *out_n = 0;
        return 0;
    }
    sqlite3_stmt *row = NULL;
    int cursor = 0;
    if (sqlite3_prepare_v2(conn,
            "SELECT last_event_id FROM kanban_notify_subs "
            "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=?",
            -1, &row, NULL) == SQLITE_OK) {
        sqlite3_bind_text(row, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 2, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 3, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 4, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(row) == SQLITE_ROW)
            cursor = sqlite3_column_type(row, 0) == SQLITE_NULL ? 0 : (int)sqlite3_column_int64(row, 0);
        sqlite3_finalize(row);
    } else return 0;

    /* Build the query with optional kind filter. */
    char qbuf[2048];
    int off = snprintf(qbuf, sizeof(qbuf),
        "SELECT * FROM task_events WHERE task_id=? AND id>?");
    if (n_kinds > 0) {
        off += snprintf(qbuf + off, sizeof(qbuf) - off, " AND kind IN (");
        for (int i = 0; i < n_kinds; i++)
            off += snprintf(qbuf + off, sizeof(qbuf) - off, i ? ",?" : "?");
        off += snprintf(qbuf + off, sizeof(qbuf) - off, ")");
    }
    off += snprintf(qbuf + off, sizeof(qbuf) - off, " ORDER BY id ASC");

    sqlite3_stmt *st = NULL;
    int n = 0, cap = 8, max_id = cursor;
    kanban_event_t **ev = malloc(sizeof(kanban_event_t*) * cap);
    if (sqlite3_prepare_v2(conn, qbuf, -1, &st, NULL) == SQLITE_OK) {
        int p = 1;
        sqlite3_bind_text(st, p++, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, p++, cursor);
        for (int i = 0; i < n_kinds; i++)
            sqlite3_bind_text(st, p++, kinds[i], -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            kanban_event_t *e = kdb_event_from_row(st);
            if (!e) continue;
            int id = kdb_event_id(e);
            if (id > max_id) max_id = id;
            if (n >= cap) { cap *= 2; ev = realloc(ev, sizeof(kanban_event_t*) * cap); }
            ev[n++] = e;
        }
        sqlite3_finalize(st);
    }
    ev = realloc(ev, sizeof(kanban_event_t*) * (n + 1));
    ev[n] = NULL;
    if (out_new_cursor) *out_new_cursor = max_id;
    if (out_events) *out_events = ev; else { kdb_event_list_free(ev); }
    if (out_n) *out_n = n;
    return n;
}

/* PoP: kdb_claim_unseen_events_for_sub @ hermes_cli/kanban_db.py:claim_unseen_events_for_sub */
int kdb_claim_unseen_events_for_sub(sqlite3 *conn, const char *task_id,
                                     const char *platform, const char *chat_id,
                                     const char *thread_id, char **kinds, int n_kinds,
                                     int *out_old_cursor, int *out_new_cursor,
                                     kanban_event_t ***out_events, int *out_n)
{
    if (!conn || !task_id || !platform || !chat_id) {
        if (out_old_cursor) *out_old_cursor = 0;
        if (out_new_cursor) *out_new_cursor = 0;
        if (out_events) *out_events = NULL;
        if (out_n) *out_n = 0;
        return 0;
    }
    if (kdb_write_begin(conn) != 0) return 0;
    int old_cursor = 0, new_cursor = 0, n = 0;
    kanban_event_t **ev = NULL;
    sqlite3_stmt *row = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT last_event_id FROM kanban_notify_subs "
            "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=?",
            -1, &row, NULL) == SQLITE_OK) {
        sqlite3_bind_text(row, 1, task_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 2, platform, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 3, chat_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(row, 4, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(row) == SQLITE_ROW)
            old_cursor = sqlite3_column_type(row, 0) == SQLITE_NULL ? 0 : (int)sqlite3_column_int64(row, 0);
        sqlite3_finalize(row);
    }
    n = kdb_unseen_events_for_sub(conn, task_id, platform, chat_id, thread_id,
                                  kinds, n_kinds, &new_cursor, &ev, out_n);
    if (n > 0) {
        sqlite3_stmt *up = NULL;
        if (sqlite3_prepare_v2(conn,
                "UPDATE kanban_notify_subs SET last_event_id=? "
                "WHERE task_id=? AND platform=? AND chat_id=? AND thread_id=? AND last_event_id=?",
                -1, &up, NULL) == SQLITE_OK) {
            sqlite3_bind_int(up, 1, new_cursor);
            sqlite3_bind_text(up, 2, task_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 3, platform, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 4, chat_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(up, 5, thread_id ? thread_id : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(up, 6, old_cursor);
            sqlite3_step(up);
            sqlite3_finalize(up);
        }
    }
    kdb_write_end(conn, 1);
    if (out_old_cursor) *out_old_cursor = old_cursor;
    if (out_new_cursor) *out_new_cursor = new_cursor;
    if (out_events) *out_events = ev; else { kdb_event_list_free(ev); }
    return n;
}

/* =========================================================================
 * Notify metadata + profile filtering + read-only probe
 * (gap closures for hermes_cli/kanban_db.py)
 * ========================================================================= */

/* ---- _encode_notify_delivery_metadata ---- */
/* Python: keep only JSON-scalar values, drop None, sort keys, compact
 * separators. Returns malloc'd compact JSON, or NULL for empty/non-mapping. */
/* PoP: kdb_encode_notify_delivery_metadata @ hermes_cli/kanban_db.py:_encode_notify_delivery_metadata */
char *kdb_encode_notify_delivery_metadata(const char *metadata_json)
{
    if (!metadata_json || !metadata_json[0]) return NULL;
    json_t *meta = json_parse(metadata_json, NULL);
    if (!meta || meta->type != JSON_OBJECT) { if (meta) json_free(meta); return NULL; }

    /* Collect scalar entries (string/number/bool), skip null. */
    size_t n = meta->c.count;
    const char **keys = malloc(sizeof(char *) * (n ? n : 1));
    json_t **vals = malloc(sizeof(json_t *) * (n ? n : 1));
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_object_get_at(meta, i);
        if (!v) continue;
        if (v->type != JSON_STRING && v->type != JSON_NUMBER && v->type != JSON_BOOL)
            continue;   /* None / nested objects / arrays dropped */
        keys[m] = json_object_get_key_at(meta, i);
        vals[m] = v;
        m++;
    }
    if (m == 0) { free(keys); free(vals); json_free(meta); return NULL; }

    /* sort_keys=True: stable qsort on key strings. */
    for (size_t i = 1; i < m; i++) {
        const char *k = keys[i];
        json_t *v = vals[i];
        size_t j = i;
        while (j > 0 && strcmp(keys[j - 1], k) > 0) {
            keys[j] = keys[j - 1];
            vals[j] = vals[j - 1];
            j--;
        }
        keys[j] = k;
        vals[j] = v;
    }

    json_t *clean = json_object();
    for (size_t i = 0; i < m; i++) {
        json_t *copy = json_string(vals[i]->type == JSON_STRING ? vals[i]->str_val : "");
        if (vals[i]->type == JSON_NUMBER) copy = json_number(vals[i]->num_val);
        else if (vals[i]->type == JSON_BOOL) copy = json_bool(vals[i]->bool_val);
        json_set(clean, keys[i], copy);
    }
    free(keys); free(vals);
    char *out = json_dumps(clean, 0);   /* compact, separators (",", ":") */
    json_free(clean);
    return out;
}

/* ---- _decode_notify_delivery_metadata ---- */
/* Python: Mapping passthrough; empty -> {}; JSON string parsed; must be a
 * dict; only scalar values survive. Returns malloc'd compact JSON ("{}" for
 * empty/unparseable). */
/* PoP: kdb_decode_notify_delivery_metadata @ hermes_cli/kanban_db.py:_decode_notify_delivery_metadata */
char *kdb_decode_notify_delivery_metadata(const char *raw)
{
    if (!raw || !raw[0]) return strdup("{}");
    json_t *data = json_parse(raw, NULL);
    if (!data || data->type != JSON_OBJECT) { if (data) json_free(data); return strdup("{}"); }
    size_t n = data->c.count;
    json_t *clean = json_object();
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_object_get_at(data, i);
        if (!v) continue;
        if (v->type != JSON_STRING && v->type != JSON_NUMBER && v->type != JSON_BOOL)
            continue;
        json_t *copy = json_string(v->type == JSON_STRING ? v->str_val : "");
        if (v->type == JSON_NUMBER) copy = json_number(v->num_val);
        else if (v->type == JSON_BOOL) copy = json_bool(v->bool_val);
        json_set(clean, json_object_get_key_at(data, i), copy);
    }
    char *out = json_dumps(clean, 0);
    json_free(clean);
    json_free(data);
    return out;
}

/* ---- _notify_profile_filter ---- */
/* Python: build "(...) OR (...)" ownership predicate + params. Returns
 * malloc'd clause; NULL when notifier_profiles is None (no filter). Params
 * are written to *out_params (NULL-terminated array, caller frees with
 * kdb_strv_free) and counted in *out_n. The "0" FALSE predicate is returned
 * when profiles are empty and include_unowned is off. */
/* PoP: kdb_notify_profile_filter @ hermes_cli/kanban_db.py:_notify_profile_filter */
char *kdb_notify_profile_filter(char **notifier_profiles, int include_unowned,
                                char ***out_params, int *out_n)
{
    if (out_params) *out_params = NULL;
    if (out_n) *out_n = 0;
    if (!notifier_profiles) return NULL;   /* None -> no filter */

    /* Dedupe + strip + sort profiles. */
    size_t cap = 8, cnt = 0;
    char **profiles = malloc(sizeof(char *) * cap);
    for (size_t i = 0; notifier_profiles[i]; i++) {
        const char *p = notifier_profiles[i];
        while (*p && isspace((unsigned char)*p)) p++;
        size_t len = strlen(p);
        while (len > 0 && isspace((unsigned char)p[len - 1])) len--;
        if (len == 0) continue;
        char *stripped = malloc(len + 1);
        memcpy(stripped, p, len);
        stripped[len] = '\0';
        int dup = 0;
        for (size_t j = 0; j < cnt; j++)
            if (strcmp(profiles[j], stripped) == 0) { dup = 1; break; }
        if (dup) { free(stripped); continue; }
        if (cnt >= cap) { cap *= 2; profiles = realloc(profiles, sizeof(char *) * cap); }
        profiles[cnt++] = stripped;
    }
    /* qsort */
    for (size_t i = 1; i < cnt; i++) {
        char *k = profiles[i];
        size_t j = i;
        while (j > 0 && strcmp(profiles[j - 1], k) > 0) {
            profiles[j] = profiles[j - 1];
            j--;
        }
        profiles[j] = k;
    }

    /* Build clauses: IN(...) when profiles exist, unowned clause when asked. */
    size_t n_clauses = 0;
    char *in_clause = NULL;
    if (cnt > 0) {
        size_t len = 18;   /* "notifier_profile IN ()" */
        for (size_t i = 0; i < cnt; i++) len += 2;
        in_clause = malloc(len);
        size_t off = snprintf(in_clause, len, "notifier_profile IN (");
        for (size_t i = 0; i < cnt; i++)
            off += snprintf(in_clause + off, len - off, i ? ",?" : "?");
        snprintf(in_clause + off, len - off, ")");
        n_clauses++;
    }
    int have_unowned = include_unowned;
    if (have_unowned) n_clauses++;

    if (n_clauses == 0) {
        for (size_t i = 0; i < cnt; i++) free(profiles[i]);
        free(profiles);
        return strdup("0");   /* FALSE predicate */
    }

    /* Assemble "(clause) OR (clause)" */
    size_t total = 1;
    for (size_t i = 0; i < n_clauses; i++) total += 2;
    if (in_clause) total += strlen(in_clause);
    total += 30;   /* unowned text */
    char *out = malloc(total);
    out[0] = '\0';
    int first = 1;
    if (in_clause) {
        snprintf(out, total, "(%s)", in_clause);
        free(in_clause);
        first = 0;
    }
    if (have_unowned) {
        if (!first) strncat(out, " OR ", total - strlen(out) - 1);
        strncat(out, "(notifier_profile IS NULL OR notifier_profile = '')",
                total - strlen(out) - 1);
    }

    /* Params: the sorted profiles (NULL-terminated array). */
    char **params = NULL;
    if (cnt > 0) {
        params = malloc(sizeof(char *) * (cnt + 1));
        for (size_t i = 0; i < cnt; i++) params[i] = strdup(profiles[i]);
        params[cnt] = NULL;
    }
    if (out_params) *out_params = params;
    if (out_n) *out_n = (int)cnt;
    for (size_t i = 0; i < cnt; i++) free(profiles[i]);
    free(profiles);
    return out;
}

/* ---- count_notify_subs ---- */
/* Python: read-only COUNT(*) probe — never creates/migrates the DB, missing
 * DB or a legacy DB without the subs table counts as zero. Returns count, or
 * -1 when an existing DB cannot be read (sqlite error other than missing
 * table). db_path explicit wins; else board via kanban_db_path. */
/* PoP: kdb_count_notify_subs @ hermes_cli/kanban_db.py:count_notify_subs */
int kdb_count_notify_subs(const char *db_path, const char *board,
                          char **notifier_profiles, int include_unowned,
                          const char *platform, const char *chat_id,
                          const char *thread_id)
{
    char *path = db_path ? strdup(db_path) : kanban_db_path(board);
    struct stat stbuf;
    if (stat(path, &stbuf) != 0) { free(path); return 0; }   /* missing -> 0 */
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(path, &conn, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        free(path);
        return -1;   /* exists but cannot be read */
    }
    char *owner_where = NULL;
    char **owner_params = NULL;
    int n_owner = 0;
    owner_where = kdb_notify_profile_filter(notifier_profiles, include_unowned,
                                            &owner_params, &n_owner);
    char clauses[2048];
    clauses[0] = '\0';
    size_t clen = 0;
    int n_params = 0;
    if (owner_where && owner_where[0]) {
        clen += snprintf(clauses + clen, sizeof(clauses) - clen, "(%s)", owner_where);
        n_params += n_owner;
    }
    if (platform && platform[0]) {
        clen += snprintf(clauses + clen, sizeof(clauses) - clen,
                         clen ? " AND LOWER(platform) = LOWER(?)" : "LOWER(platform) = LOWER(?)");
        n_params++;
    }
    if (chat_id && chat_id[0]) {
        clen += snprintf(clauses + clen, sizeof(clauses) - clen,
                         clen ? " AND chat_id = ?" : "chat_id = ?");
        n_params++;
    }
    if (thread_id && thread_id[0]) {
        clen += snprintf(clauses + clen, sizeof(clauses) - clen,
                         clen ? " AND thread_id = ?" : "thread_id = ?");
        n_params++;
    }
    char query[4096];
    if (clauses[0])
        snprintf(query, sizeof(query), "SELECT COUNT(*) FROM kanban_notify_subs WHERE %s", clauses);
    else
        snprintf(query, sizeof(query), "SELECT COUNT(*) FROM kanban_notify_subs");

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(conn, query, -1, &st, NULL);
    if (rc != SQLITE_OK) {
        const char *msg = sqlite3_errmsg(conn);
        int is_no_table = msg && strstr(msg, "no such table") != NULL;
        sqlite3_finalize(st);
        sqlite3_close(conn);
        if (owner_where) free(owner_where);
        if (owner_params) kdb_strv_free(owner_params);
        free(path);
        return is_no_table ? 0 : -1;
    }
    int p = 1;
    if (owner_params) {
        for (int i = 0; i < n_owner; i++)
            sqlite3_bind_text(st, p++, owner_params[i], -1, SQLITE_TRANSIENT);
    }
    if (platform && platform[0]) sqlite3_bind_text(st, p++, platform, -1, SQLITE_TRANSIENT);
    if (chat_id && chat_id[0]) sqlite3_bind_text(st, p++, chat_id, -1, SQLITE_TRANSIENT);
    if (thread_id && thread_id[0]) sqlite3_bind_text(st, p++, thread_id, -1, SQLITE_TRANSIENT);

    int count = 0;
    if (sqlite3_step(st) == SQLITE_ROW)
        count = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    sqlite3_close(conn);
    if (owner_where) free(owner_where);
    if (owner_params) kdb_strv_free(owner_params);
    free(path);
    return count;
}

/* ---- _inherit_notify_subs ---- */
/* Python: copy gateway notify subs from parent tasks to a child, starting
 * the child's subscription caught up to its current event cursor. Dedupes
 * parents preserving order; skips empty ids. Returns 0 on success, -1 on
 * sqlite failure (INSERT OR IGNORE — no-op when child already subscribed). */
/* PoP: kdb_inherit_notify_subs @ hermes_cli/kanban_db.py:_inherit_notify_subs */
int kdb_inherit_notify_subs(sqlite3 *conn, const char *child_id,
                            char **parents, long created_at)
{
    if (!conn || !child_id || !child_id[0]) return -1;
    if (!parents) return 0;

    /* Dedupe parent ids, preserving order, dropping empties. */
    size_t cap = 8, cnt = 0;
    char **parent_ids = malloc(sizeof(char *) * cap);
    for (size_t i = 0; parents[i]; i++) {
        if (!parents[i][0]) continue;
        int dup = 0;
        for (size_t j = 0; j < cnt; j++)
            if (strcmp(parent_ids[j], parents[i]) == 0) { dup = 1; break; }
        if (dup) continue;
        if (cnt >= cap) { cap *= 2; parent_ids = realloc(parent_ids, sizeof(char *) * cap); }
        parent_ids[cnt++] = parents[i];
    }
    if (cnt == 0) { free(parent_ids); return 0; }

    /* Cursor: MAX(task_events.id) for the child, 0 when no events. */
    long cursor = 0;
    sqlite3_stmt *row = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT COALESCE(MAX(id), 0) FROM task_events WHERE task_id = ?",
            -1, &row, NULL) == SQLITE_OK) {
        sqlite3_bind_text(row, 1, child_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(row) == SQLITE_ROW)
            cursor = sqlite3_column_int64(row, 0);
        sqlite3_finalize(row);
    }

    long now = created_at > 0 ? created_at : kdb_now();
    /* INSERT...SELECT with N placeholders for parent ids. */
    size_t qlen = 256 + cnt * 2;
    char *q = malloc(qlen);
    size_t off = snprintf(q, qlen,
        "INSERT OR IGNORE INTO kanban_notify_subs "
        "(task_id, platform, chat_id, thread_id, user_id, notifier_profile, created_at, last_event_id) "
        "SELECT ?, platform, chat_id, thread_id, user_id, notifier_profile, ?, ? "
        "FROM kanban_notify_subs WHERE task_id IN (");
    for (size_t i = 0; i < cnt; i++)
        off += snprintf(q + off, qlen - off, i ? ",?" : "?");
    snprintf(q + off, qlen - off, ")");

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(conn, q, -1, &st, NULL);
    free(q);
    if (rc != SQLITE_OK) {
        free(parent_ids);
        return -1;
    }
    sqlite3_bind_text(st, 1, child_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, now);
    sqlite3_bind_int64(st, 3, cursor);
    for (size_t i = 0; i < cnt; i++)
        sqlite3_bind_text(st, (int)(4 + i), parent_ids[i], -1, SQLITE_TRANSIENT);
    int done = (sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    free(parent_ids);
    return done ? 0 : -1;
}
