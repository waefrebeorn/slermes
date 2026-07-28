/*
 * port_web_server_session_endpoints.c — HTTP-shape endpoint wrappers for the
 * session-detail family: a faithful C11 port of the web_server.py handlers
 * (get_session_detail, get_session_messages, delete_session_endpoint,
 * rename_session_endpoint, export_session_endpoint,
 * _session_latest_descendant).
 *
 * These are thin wrappers over the SessionDB plumbing in
 * port_web_server_session_detail.c (via web_server_session_detail.h). They
 * reuse the same db_path adapter pattern and libjson JSON — no duplicated
 * SQL, no god header. The routing layer maps the embedded integers to real
 * HTTP status codes.
 */

#include "web_server_session_detail.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "sqlite3.h"

/* Shared by all wrappers: {"status":N,"detail":s} error shape. */
static json_t *err_obj(int status, const char *detail) {
    json_t *o = json_object();
    json_set(o, "status", json_number((double)status));
    json_set(o, "detail", json_string(detail));
    return o;
}

/* PoP: ws_sess_detail_endpoint @ hermes_cli/web_server.py:get_session_detail */
json_t *ws_sess_detail_endpoint(const char *db_path, const char *session_id) {
    json_t *sess = ws_sess_get_session(db_path, session_id);
    if (!sess) return err_obj(404, "Session not found");
    return sess;
}

/* PoP: ws_sess_messages_endpoint @ hermes_cli/web_server.py:get_session_messages */
json_t *ws_sess_messages_endpoint(const char *db_path, const char *session_id,
                                  bool has_limit, int limit, int offset) {
    char *sid = strdup(session_id ? session_id : "");
    char *resolved = ws_sess_resolve_resume_id(db_path, sid); /* may refine */
    free(sid);
    /* Clamp limit to 500 (mirrors Python min(limit,500)). */
    int eff_limit = has_limit ? (limit > 500 ? 500 : limit) : -1;
    json_t *arr = ws_sess_get_messages(db_path, resolved,
                                       false, has_limit, eff_limit, offset);
    free(resolved);
    json_t *out = json_object();
    json_set(out, "session_id", json_string(session_id));
    json_set(out, "messages", arr);
    json_t *pg = json_object();
    json_set(pg, "limit", has_limit ? json_number((double)eff_limit) : json_null());
    json_set(pg, "offset", json_number((double)offset));
    json_set(pg, "returned", json_number((double)json_len(arr)));
    json_set(out, "pagination", pg);
    return out;
}

/* PoP: ws_sess_delete_endpoint @ hermes_cli/web_server.py:delete_session_endpoint */
json_t *ws_sess_delete_endpoint(const char *db_path, const char *session_id) {
    /* Exact-id (prefix resolution handled by routing layer). Ghost ids are
     * idempotent success. */
    bool deleted = ws_sess_delete_session(db_path, session_id);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    if (!deleted) json_set(out, "already_absent", json_bool(true));
    return out;
}

/* PoP: ws_sess_rename_endpoint @ hermes_cli/web_server.py:rename_session_endpoint */
json_t *ws_sess_rename_endpoint(const char *db_path, const char *session_id,
                                const json_t *body) {
    json_t *sess = ws_sess_get_session(db_path, session_id);
    if (!sess) { json_free(sess); return err_obj(404, "Session not found"); }
    json_free(sess);

    json_t *title_v = body ? json_obj_get(body, "title") : NULL;
    json_t *arch_v  = body ? json_obj_get(body, "archived") : NULL;
    json_t *pin_v   = body ? json_obj_get(body, "pinned") : NULL;

    if (!title_v && !arch_v && !pin_v)
        return err_obj(400, "Nothing to update; provide 'title', 'archived', and/or 'pinned'.");

    if (title_v) {
        if (title_v->type != JSON_NULL && title_v->type != JSON_STRING)
            return err_obj(400, "title must be a string or null");
        const char *t = (title_v->type == JSON_STRING) ? title_v->str_val : "";
        char *err = NULL;
        if (!ws_sess_set_title(db_path, session_id, t ? t : "", &err)) {
            if (err && strncmp(err, "Title too long", 14) == 0) {
                free(err);
                return err_obj(400, "Title too long (100 chars, max 100)");
            }
            free(err);
            return err_obj(400, "Title already in use by another session");
        }
    }
    if (arch_v && arch_v->type == JSON_BOOL)
        ws_sess_set_archived(db_path, session_id, arch_v->bool_val);
    if (pin_v && pin_v->type == JSON_BOOL)
        ws_sess_set_pinned(db_path, session_id, pin_v->bool_val);

    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    char *title = ws_sess_get_title(db_path, session_id);
    json_set(out, "title", json_string(title ? title : ""));
    free(title);
    if (arch_v && arch_v->type == JSON_BOOL)
        json_set(out, "archived", json_bool(arch_v->bool_val));
    if (pin_v && pin_v->type == JSON_BOOL)
        json_set(out, "pinned", json_bool(pin_v->bool_val));
    return out;
}

/* PoP: ws_sess_export_endpoint @ hermes_cli/web_server.py:export_session_endpoint */
json_t *ws_sess_export_endpoint(const char *db_path, const char *session_id) {
    json_t *data = ws_sess_export_session(db_path, session_id);
    if (!data) return err_obj(404, "Session not found");
    return data;
}

/* PoP: ws_sess_latest_descendant_endpoint @ hermes_cli/web_server.py:_session_latest_descendant */
json_t *ws_sess_latest_descendant_endpoint(const char *db_path,
                                           const char *session_id) {
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return err_obj(404, "Session not found");
    /* Confirm exists. */
    sqlite3_stmt *st = NULL;
    bool exists = false;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM sessions WHERE id = ?",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        exists = (sqlite3_step(st) == SQLITE_ROW);
    }
    sqlite3_finalize(st);
    if (!exists) { sqlite3_close(db); return err_obj(404, "Session not found"); }

    /* Load full descendant tree. */
    char **ids = NULL; char **par = NULL; double *sta = NULL;
    size_t n = 0, cap = 0;
    if (sqlite3_prepare_v2(db,
            "WITH RECURSIVE descendants(id, parent_session_id, started_at) AS ("
            "  SELECT id, parent_session_id, started_at FROM sessions WHERE id = ? "
            "  UNION "
            "  SELECT s.id, s.parent_session_id, s.started_at FROM sessions s "
            "  JOIN descendants d ON s.parent_session_id = d.id) "
            "SELECT id, parent_session_id, started_at FROM descendants",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (n == cap) { cap = cap ? cap * 2 : 16;
                ids = realloc(ids, cap * sizeof *ids);
                par = realloc(par, cap * sizeof *par);
                sta = realloc(sta, cap * sizeof *sta); }
            const unsigned char *i = sqlite3_column_text(st, 0);
            const unsigned char *p = sqlite3_column_text(st, 1);
            ids[n] = strdup(i ? (const char *)i : "");
            par[n] = strdup(p ? (const char *)p : "");
            sta[n] = sqlite3_column_double(st, 2);
            n++;
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);

    /* Build child map. */
    char **children[128]; size_t childn[128];
    for (size_t i = 0; i < n && i < 128; i++) { children[i] = NULL; childn[i] = 0; }
    for (size_t i = 0; i < n; i++) {
        if (!par[i] || !*par[i]) continue;
        for (size_t j = 0; j < n; j++) {
            if (j == i) continue;
            if (strcmp(par[i], ids[j]) == 0) {
                /* parent of i is j → i is a child of j */
                children[j] = realloc(children[j], (childn[j] + 1) * sizeof(char*));
                children[j][childn[j]++] = ids[i];
            }
        }
    }
    char *current = strdup(session_id);
    char **path = malloc((n + 1) * sizeof(char*));
    size_t pn = 0;
    path[pn++] = strdup(session_id);
    char *seen[128]; int seen_n = 0;
    while (true) {
        int ci = -1;
        for (size_t j = 0; j < n; j++)
            if (strcmp(ids[j], current) == 0) { ci = (int)j; break; }
        if (ci < 0) break;
        if (childn[ci] == 0) break;
        /* pick latest-started unseen child */
        int best = -1; double best_sta = -1;
        for (size_t k = 0; k < childn[ci]; k++) {
            bool s = false;
            for (int ss = 0; ss < seen_n; ss++)
                if (strcmp(seen[ss], children[ci][k]) == 0) s = true;
            if (s) continue;
            int idx = -1;
            for (size_t j = 0; j < n; j++)
                if (strcmp(ids[j], children[ci][k]) == 0) { idx = (int)j; break; }
            if (idx < 0) continue;
            if (sta[idx] > best_sta) { best_sta = sta[idx]; best = idx; }
        }
        if (best < 0) break;
        bool s2 = false;
        for (int ss = 0; ss < seen_n; ss++)
            if (strcmp(seen[ss], ids[best]) == 0) s2 = true;
        if (s2) break;
        if (seen_n < 128) seen[seen_n++] = strdup(ids[best]);
        free(current);
        current = strdup(ids[best]);
        path[pn++] = strdup(ids[best]);
    }

    json_t *out = json_object();
    json_set(out, "requested_session_id", json_string(session_id));
    json_set(out, "session_id", json_string(current));
    json_t *parr = json_array();
    for (size_t i = 0; i < pn; i++) json_append(parr, json_string(path[i]));
    json_set(out, "path", parr);
    json_set(out, "changed", json_bool(pn > 1 && strcmp(current, session_id) != 0));

    free(current);
    for (size_t i = 0; i < pn; i++) free(path[i]);
    free(path);
    for (size_t i = 0; i < n; i++) { free(ids[i]); free(par[i]); }
    free(ids); free(par); free(sta);
    for (size_t i = 0; i < n && i < 128; i++) free(children[i]);
    for (int i = 0; i < seen_n; i++) free(seen[i]);
    return out;
}
