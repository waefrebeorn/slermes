#ifndef SESSION_DB_H
#define SESSION_DB_H

#include "app_state.h"
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * Session Database API
 * ══════════════════════════════════════════════════════════════════════ */

/* Load all sessions from database */
void session_db_load_sessions(app_state_t *app);

/* Load messages for a specific session */
void session_db_load_messages(app_state_t *app, int idx);

/* Get session by index */
app_session_entry_t *session_db_get_session(app_state_t *app, int idx);

/* Get message by index */
message_entry_t *session_db_get_message(app_state_t *app, int idx);

/* Update session model */
bool session_db_update_session_model(app_state_t *app, int session_idx, const char *model);

/* Delete a session */
bool session_db_delete_session(app_state_t *app, int session_idx);

/* Archive/unarchive a session */
bool session_db_archive_session(app_state_t *app, int session_idx, bool archive);

/* Pin/unpin a session */
bool session_db_pin_session(app_state_t *app, int session_idx, bool pin);

/* Create a new session */
int session_db_create_session(app_state_t *app, const char *title, const char *source, const char *model);

/* Create a session with an explicit ID (used by session import). The app
 * pointer may be NULL (no in-memory reload). Returns 1 on success. */
int session_db_create_named(app_state_t *app, const char *id, const char *title,
                            const char *source, const char *model);

/* Insert a message into an existing session (used by session import).
 * Returns 1 on success. */
int session_db_insert_message(const char *session_id, const char *role,
                              const char *content, double timestamp);

/* Load skills from filesystem */
void session_db_load_skills(app_state_t *app);

/* Load profiles from filesystem */
void session_db_load_profiles(app_state_t *app);

/* Load cron jobs from filesystem */
void session_db_load_cron(app_state_t *app);

/* Load statistics */
void session_db_load_stats(app_state_t *app);

/* Open database */
int session_db_open(app_state_t *app);

/* Close database */
void session_db_close(app_state_t *app);

#endif /* SESSION_DB_H */