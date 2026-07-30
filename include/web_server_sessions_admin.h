/*
 * web_server_sessions_admin.h — sessions admin cluster (faithful C11 port
 * of count_empty_sessions / delete_empty_sessions / session_count /
 * message_count from hermes_state.py and the get_session_stats endpoint
 * aggregation from hermes_cli/web_server.py).
 *
 * All functions open the sqlite DB at db_path read-only (or read-write for
 * the delete) per call — the dashboard endpoints open a fresh SessionDB per
 * request the same way.
 */
#ifndef WEB_SERVER_SESSIONS_ADMIN_H
#define WEB_SERVER_SESSIONS_ADMIN_H

#include <stdbool.h>

#include "libjson/json.h"

/* SessionDB.count_empty_sessions */
int ws_sessions_count_empty(const char *db_path);

/* SessionDB.delete_empty_sessions (no sessions_dir sweep — the web endpoint
 * passes none). Returns number deleted, -1 on open failure. */
int ws_sessions_delete_empty(const char *db_path);

/* SessionDB.message_count; session_id NULL = all messages. */
int ws_sessions_message_count(const char *db_path, const char *session_id);

/* SessionDB.session_count with the dashboard-relevant filters. */
typedef struct {
    const char *source;          /* NULL = no filter */
    bool include_archived;
    bool archived_only;
    bool exclude_children;
    int min_message_count;
} ws_session_count_opts_t;

int ws_sessions_session_count(const char *db_path,
                              const ws_session_count_opts_t *opts);

/* get_session_stats endpoint: {"total","active_store","archived",
 * "messages","by_source"} — by_source over the same listable-rows
 * projection list_sessions_rich(include_archived=True) surfaces. */
json_t *ws_sessions_stats(const char *db_path);

#endif /* WEB_SERVER_SESSIONS_ADMIN_H */
