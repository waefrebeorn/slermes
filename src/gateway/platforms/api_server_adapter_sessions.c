/**
 * api_server_adapter_sessions.c — Session management handlers.
 * Port of Python: gateway/platforms/api_server.py session endpoints
 */

#include "api_server_adapter.h"
#include "api_server_session_json.h"
#include "hermes_json.h"
#include "hermes_gateway_webhook.h"
#include "hermes_gateway_config.h"
#include "../lib/libdb/db.h"  /* Use libdb directly */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

extern api_server_adapter_t *g_api_adapter;  /* Global for session DB access */

/* ── Session Endpoints ────────────────────────────────────────────── */

void api_server_handle_list_sessions(api_server_adapter_t *adapter, int client_fd, const char *query)
{
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    db_t *db = get_session_db();
    if (!db) { send_error_response(client_fd, 503, "Session database unavailable", "session_db_unavailable"); return; }

    int limit = parse_nonnegative_int(strstr(query, "limit=") ? strstr(query, "limit=") + 6 : NULL, 50, 200);
    int offset = parse_nonnegative_int(strstr(query, "offset=") ? strstr(query, "offset=") + 7 : NULL, 0, 1000000);

    char *source = NULL;
    if (strstr(query, "source=")) {
        source = strdup(strstr(query, "source=") + 7);
        char *amp = strchr(source, '&');
        if (amp) *amp = '\0';
    }

    bool include_children = strstr(query, "include_children=true") != NULL;

    size_t count = 0;
    db_session_entry_t *entries = db_list_with_meta(db, &count);
    free(source);

    json_t *root = json_object();
    json_set(root, "object", json_string("list"));
    json_t *data = json_array();

    if (entries) {
        size_t start = (offset >= 0 && (size_t)offset < count) ? (size_t)offset : 0;
        size_t end = (start + (size_t)limit < count) ? start + (size_t)limit : count;
        for (size_t i = start; i < end; i++) {
            json_t *s = session_to_json(db, entries[i].id);
            if (s) json_append(data, s);
        }
        free(entries);
    }

    json_set(root, "data", data);
    json_set(root, "limit", json_number(limit));
    json_set(root, "offset", json_number(offset));
    json_set(root, "has_more", json_bool(entries && count > (size_t)(offset + limit)));

    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
}

void api_server_handle_create_session(api_server_adapter_t *adapter, int client_fd, const char *body)
{
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!body || !*body) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    json_t *req = json_parse(strdup(body), NULL);
    if (!req) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    db_t *db = get_session_db();
    if (!db) { json_free(req); send_error_response(client_fd, 503, "Session database unavailable", "session_db_unavailable"); return; }

    const char *raw_id = json_get_str(req, "id", "");
    if (!*raw_id) raw_id = json_get_str(req, "session_id", "");
    char session_id[128];
    if (*raw_id) {
        strncpy(session_id, raw_id, sizeof(session_id) - 1);
        session_id[sizeof(session_id) - 1] = '\0';
    } else {
        char *uuid_str = uuid_v4();
        if (!uuid_str) uuid_str = strdup("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx");
        /* Format: YYYYMMDD_HHMMSS_xxxxxxxx */
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        snprintf(session_id, sizeof(session_id), "%04d%02d%02d_%02d%02d%02d_%s",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec, uuid_str + 24);  /* last 12 chars */
        free(uuid_str);
    }

    if (!session_id[0] || strpbrk(session_id, "\r\n\0")) {
        json_free(req);
        send_error_response(client_fd, 400, "Invalid session ID", "invalid_session_id");
        return;
    }

    if (strlen(session_id) > MAX_SESSION_HEADER_LEN) {
        json_free(req);
        send_error_response(client_fd, 400, "Session ID too long", "invalid_session_id");
        return;
    }

    if (db_exists(db, session_id)) {
        json_free(req);
        send_error_response(client_fd, 409, "Session already exists", "session_exists");
        return;
    }

    const char *model = json_get_str(req, "model", "");
    if (!*model) model = adapter->model_name;

    const char *system_prompt = json_get_str(req, "system_prompt", "");
    const char *title = json_get_str(req, "title", "");

    /* Create empty session data */
    json_t *session_data = json_object();
    json_set(session_data, "session_id", json_string(session_id));
    json_set(session_data, "messages", json_array());
    char *serialized = json_serialize(session_data);
    json_free(session_data);
    db_save(db, session_id, serialized);
    free(serialized);

    /* Save metadata */
    session_meta_t meta;
    db_meta_init(&meta);
    strncpy(meta.title, title[0] ? title : "New Session", sizeof(meta.title) - 1);
    strncpy(meta.model, model, sizeof(meta.model) - 1);
    strncpy(meta.source, "api_server", sizeof(meta.source) - 1);
    meta.message_count = 0;
    meta.created_at = time(NULL);
    meta.updated_at = time(NULL);
    db_save_meta(db, session_id, &meta);

    json_t *session = session_to_json(db, session_id);
    json_free(req);

    json_t *root = json_object();
    json_set(root, "object", json_string("hermes.session"));
    json_set(root, "session", session);
    char *out = json_serialize(root);
    send_json_response(client_fd, 201, out);
    free(out);
    json_free(root);
}

void api_server_handle_get_session(api_server_adapter_t *adapter, int client_fd, const char *session_id)
{
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    db_t *db = get_session_db();
    if (!db) { send_error_response(client_fd, 503, "Session database unavailable", "session_db_unavailable"); return; }

    json_t *session = session_to_json(db, session_id);
    if (!session) { send_error_response(client_fd, 404, "Session not found", "session_not_found"); return; }

    json_t *root = json_object();
    json_set(root, "object", json_string("hermes.session"));
    json_set(root, "session", session);
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
}

void api_server_handle_patch_session(api_server_adapter_t *adapter, int client_fd, const char *session_id, const char *body)
{
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    db_t *db = get_session_db();
    if (!db) { send_error_response(client_fd, 503, "Session database unavailable", "session_db_unavailable"); return; }

    if (!db_exists(db, session_id)) { send_error_response(client_fd, 404, "Session not found", "session_not_found"); return; }

    if (!body || !*body) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    json_t *req = json_parse(strdup(body), NULL);
    if (!req) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    const char *title = json_get_str(req, "title", NULL);
    const char *end_reason = json_get_str(req, "end_reason", NULL);

    /* Check for unknown fields */
    const char *allowed[] = {"title", "end_reason", NULL};
    for (size_t i = 0; i < req->c.count; i++) {
        const char *key = req->c.keys[i];
        if (!key) continue;
        bool ok = false;
        for (int j = 0; allowed[j]; j++) {
            if (strcmp(key, allowed[j]) == 0) { ok = true; break; }
        }
        if (!ok) {
            send_error_response(client_fd, 400, "Unsupported session field", "unsupported_session_field");
            json_free(req);
            return;
        }
    }

    session_meta_t meta;
    if (db_load_meta(db, session_id, &meta)) {
        if (title) {
            strncpy(meta.title, title, sizeof(meta.title) - 1);
            meta.updated_at = time(NULL);
        }
        if (end_reason && *end_reason) {
            strncpy(meta.end_reason, end_reason, sizeof(meta.end_reason) - 1);
            meta.ended_at = time(NULL);
            meta.updated_at = time(NULL);
        }
        db_save_meta(db, session_id, &meta);
    }

    json_t *session = session_to_json(db, session_id);
    json_free(req);

    json_t *root = json_object();
    json_set(root, "object", json_string("hermes.session"));
    json_set(root, "session", session);
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
}

void api_server_handle_delete_session(api_server_adapter_t *adapter, int client_fd, const char *session_id)
{
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    db_t *db = get_session_db();
    if (!db) { send_error_response(client_fd, 503, "Session database unavailable", "session_db_unavailable"); return; }

    if (!db_exists(db, session_id)) { send_error_response(client_fd, 404, "Session not found", "session_not_found"); return; }

    bool deleted = db_delete(db, session_id);

    json_t *root = json_object();
    json_set(root, "object", json_string("hermes.session.deleted"));
    json_set(root, "id", json_string(session_id));
    json_set(root, "deleted", json_bool(deleted));
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
}

void api_server_handle_session_messages(api_server_adapter_t *adapter, int client_fd, const char *session_id)
{
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    db_t *db = get_session_db();
    if (!db) { send_error_response(client_fd, 503, "Session database unavailable", "session_db_unavailable"); return; }

    char *session_data = db_load(db, session_id, NULL);
    if (!session_data) { send_error_response(client_fd, 404, "Session not found", "session_not_found"); return; }

    char *err = NULL;
    json_t *root = json_parse(session_data, &err);
    free(session_data);
    if (!root) { send_error_response(client_fd, 500, "Session data corrupted", "session_data_corrupted"); return; }

    json_t *msgs = json_object_get(root, "messages");
    if (!msgs || msgs->type != JSON_ARRAY) {
        json_free(root);
        send_error_response(client_fd, 500, "Session data corrupted", "session_data_corrupted");
        return;
    }

    json_t *out_root = json_object();
    json_set(out_root, "object", json_string("list"));
    json_set(out_root, "session_id", json_string(session_id));
    json_t *data = json_array();

    size_t count = json_array_count(msgs);
    for (size_t i = 0; i < count; i++) {
        json_t *msg = json_array_get(msgs, i);
        char *msg_str = json_serialize(msg);
        if (msg_str) {
            json_t *m = message_to_json(msg_str);
            if (m) json_append(data, m);
            free(msg_str);
        }
    }

    json_set(out_root, "data", data);
    char *out = json_serialize(out_root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(out_root);
    json_free(root);
}

void api_server_handle_fork_session(api_server_adapter_t *adapter, int client_fd, const char *session_id, const char *body)
{
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    db_t *db = get_session_db();
    if (!db) { send_error_response(client_fd, 503, "Session database unavailable", "session_db_unavailable"); return; }

    if (!db_exists(db, session_id)) { send_error_response(client_fd, 404, "Session not found", "session_not_found"); return; }

    int branch_point = -1;
    if (body && *body) {
        json_t *req = json_parse(strdup(body), NULL);
        if (req) {
            json_t *bp = json_object_get(req, "branch_point");
            if (bp && bp->type == JSON_NUMBER) branch_point = (int)bp->num_val;
            json_free(req);
        }
    }

    /* Generate new session ID */
    char new_id[128];
    char *uuid_str = uuid_v4();
    if (!uuid_str) uuid_str = strdup("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx");
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(new_id, sizeof(new_id), "%04d%02d%02d_%02d%02d%02d_%s",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec, uuid_str + 24);
    free(uuid_str);

    char *forked_data = NULL;
    if (branch_point >= 0) {
        forked_data = db_branch(db, session_id, new_id, branch_point);
    } else {
        char *src = db_load(db, session_id, NULL);
        if (src) {
            db_save(db, new_id, src);
            forked_data = db_load(db, new_id, NULL);
            free(src);
        }
    }

    if (!forked_data) { send_error_response(client_fd, 404, "Session not found or fork failed", NULL); return; }

    /* Copy metadata with parent link */
    session_meta_t meta;
    if (db_load_meta(db, session_id, &meta)) {
        db_meta_init(&meta);
        strncpy(meta.parent_id, session_id, sizeof(meta.parent_id) - 1);
        meta.branch_point = branch_point;
        snprintf(meta.title, sizeof(meta.title), "Fork of %s", session_id);
        meta.created_at = time(NULL);
        meta.updated_at = time(NULL);
        db_save_meta(db, new_id, &meta);
    }

    json_t *root = json_object();
    json_set(root, "id", json_string(new_id));
    json_set(root, "parent_id", json_string(session_id));
    json_set(root, "branch_point", json_number(branch_point));
    char *out = json_serialize(root);
    send_json_response(client_fd, 201, out);
    free(out);
    json_free(root);
    free(forked_data);
}

/* End of api_server_adapter_sessions.c */