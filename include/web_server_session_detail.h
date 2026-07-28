/*
 * web_server_session_detail.h — session detail/messages/rename/export/
 * delete endpoint family (faithful C11 port of the SessionDB methods in
 * hermes_state.py + the endpoint wrappers in hermes_cli/web_server.py).
 */
#ifndef WEB_SERVER_SESSION_DETAIL_H
#define WEB_SERVER_SESSION_DETAIL_H

#include <stdbool.h>

#include "libjson/json.h"

/* Opaque sqlite handle — we only pass pointers across this boundary, so a
 * forward declaration keeps the header minimal (no sqlite3.h leak). */
typedef struct sqlite3 sqlite3;

/* Shared adapter: open the sqlite store read-only (rw=false) or read/write.
 * Returns NULL on failure. Used by both the plumbing
 * (port_web_server_session_detail.c) and the endpoint wrappers
 * (port_web_server_session_endpoints.c) so the db_path pattern stays in one
 * place. */
sqlite3 *ws_sess_open_db(const char *path, bool rw);

/* SessionDB.get_session: SELECT * row as object, or NULL. */
json_t *ws_sess_get_session(const char *db_path, const char *session_id);

/* SessionDB.get_messages with content/tool_calls/display_metadata decode.
 * has_limit=false → limit None. */
json_t *ws_sess_get_messages(const char *db_path, const char *session_id,
                             bool include_inactive, bool has_limit,
                             int limit, int offset);

/* SessionDB.get_compression_tip / resolve_resume_session_id.
 * Return malloc'd id (may equal input). */
char *ws_sess_compression_tip(const char *db_path, const char *session_id);
char *ws_sess_resolve_resume_id(const char *db_path, const char *session_id);

/* SessionDB.delete_session (no sessions_dir / expected ids — endpoint
 * parity). Returns true when found and deleted. */
bool ws_sess_delete_session(const char *db_path, const char *session_id);

/* SessionDB.sanitize_title. Returns malloc'd cleaned title or NULL
 * (empty). On too-long returns NULL and sets *err (malloc'd). */
char *ws_sess_sanitize_title(const char *title, char **err);

/* SessionDB.set_session_title. On ValueError returns false with *err. */
bool ws_sess_set_title(const char *db_path, const char *session_id,
                       const char *title, char **err);

/* SessionDB.get_session_title: malloc'd or NULL. */
char *ws_sess_get_title(const char *db_path, const char *session_id);

/* SessionDB.set_session_archived / set_session_pinned (compression-lineage
 * CTE update). Return true when >0 rows changed. */
bool ws_sess_set_archived(const char *db_path, const char *session_id,
                          bool archived);
bool ws_sess_set_pinned(const char *db_path, const char *session_id,
                        bool pinned);

/* SessionDB.export_session: {**session, "messages": [...]} or NULL. */
json_t *ws_sess_export_session(const char *db_path, const char *session_id);

/* Endpoint wrappers (web_server.py). Error shape {"status":N,"detail":s}. */
json_t *ws_sess_detail_endpoint(const char *db_path, const char *session_id);
json_t *ws_sess_messages_endpoint(const char *db_path,
                                  const char *session_id, bool has_limit,
                                  int limit, int offset);
json_t *ws_sess_delete_endpoint(const char *db_path, const char *session_id);
json_t *ws_sess_rename_endpoint(const char *db_path, const char *session_id,
                                const json_t *body);
json_t *ws_sess_export_endpoint(const char *db_path, const char *session_id);
json_t *ws_sess_latest_descendant_endpoint(const char *db_path,
                                           const char *session_id);

#endif /* WEB_SERVER_SESSION_DETAIL_H */
