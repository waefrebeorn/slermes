/* hermes_state_db.h — faithful C11 port of the session/state surface of
 * hermes_state.py (the SessionDB class), backed by the real sqlite state.db
 * schema (sessions + messages + session_model_usage). This is the stack that
 * the agent loop, session listing, resume, compression, and analytics all
 * read/write through — the single source of truth for session state in C.
 *
 * Every method here is a byte-faithful mirror of its Python counterpart's
 * SQL + control flow. Implemented in src/agent/hermes_state/ over the
 * vendored sqlite3 (lib/libdb/sqlite3.c). Opaque handle; no god headers.
 */

#ifndef SLERMES_HERMES_STATE_DB_H
#define SLERMES_HERMES_STATE_DB_H

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hermes_state_db hermes_state_db_t;

/* Open (and ensure schema for) the state.db at *path* (mirror of SessionDB's
 * sqlite init: sessions + messages + session_model_usage + schema_version).
 * Returns NULL on failure. */
hermes_state_db_t *hermes_state_db_open(const char *path);
void hermes_state_db_close(hermes_state_db_t *db);

/* ── Session lifecycle ──────────────────────────────────────────────────
 * PoP: create_session @ hermes_state.py:create_session
 * Insert (INSERT OR IGNORE) a session row; returns the id. */
bool hermes_state_create_session(hermes_state_db_t *db, const char *session_id,
                                 const char *source);

/* PoP: end_session @ hermes_state.py:end_session
 * Set ended_at/end_reason only when not already ended (first writer wins). */
bool hermes_state_end_session(hermes_state_db_t *db, const char *session_id,
                              const char *end_reason);

/* PoP: set_session_archived @ hermes_state.py:set_session_archived */
bool hermes_state_set_session_archived(hermes_state_db_t *db,
                                       const char *session_id, bool archived);

/* PoP: get_session @ hermes_state.py:get_session
 * Returns malloc'd JSON object string of the session row, or NULL when
 * absent. Caller frees. */
char *hermes_state_get_session(hermes_state_db_t *db, const char *session_id);

/* Test/lifecycle helper: link a child session under a parent and mark it
 * ended with a compression reason (mirrors Python's compression-rotation
 * UPDATE sessions SET parent_session_id=?, end_reason=?, ended_at=?). */
bool hermes_state_link_child(hermes_state_db_t *db, const char *child_id,
                             const char *parent_id, const char *end_reason);

/* Test/lifecycle helper: set a session's model_config JSON (mark delegate/
 * branch children that the compression walk must exclude). */
bool hermes_state_set_model_config(hermes_state_db_t *db, const char *session_id,
                                   const char *model_config_json);

/* ── Message append / read ──────────────────────────────────────────────
 * PoP: append_message @ hermes_state.py:append_message
 * Appends a message row and bumps message_count (and tool_call_count when
 * role is 'tool' or tool_calls present). Returns the new row id, or -1. */
long long hermes_state_append_message(hermes_state_db_t *db,
                                      const char *session_id,
                                      const char *role,
                                      const char *content,
                                      const char *tool_name,
                                      const char *tool_call_id,
                                      int token_count);

/* PoP: get_messages_around @ hermes_state.py:get_messages_around
 * Returns a malloc'd JSON: {"window":[{id,role,content,...}],
 * "messages_before":N,"messages_after":M}. Empty window when the anchor is
 * not in the session. Caller frees. */
char *hermes_state_get_messages_around(hermes_state_db_t *db,
                                       const char *session_id,
                                       long long around_message_id,
                                       int window);

/* PoP: get_anchored_view @ hermes_state.py:get_anchored_view
 * window + bookend_start + bookend_end (user/assistant filtered), anchor
 * always preserved. Returns malloc'd JSON. Caller frees. */
char *hermes_state_get_anchored_view(hermes_state_db_t *db,
                                     const char *session_id,
                                     long long around_message_id,
                                     int window, int bookend);

/* ── Lineage / resume ───────────────────────────────────────────────────
 * PoP: get_conversation_root @ hermes_state.py:get_conversation_root
 * Walk parent_session_id to the root; returns malloc'd id (caller frees). */
char *hermes_state_get_conversation_root(hermes_state_db_t *db,
                                         const char *session_id);

/* PoP: get_compression_tip @ hermes_state.py:get_compression_tip
 * Walk compression-continuation chain (children of compression-ended parents,
 * excluding branched/delegate/tool children) to the tip. Returns malloc'd id
 * (caller frees); returns strdup(session_id) when no continuation. */
char *hermes_state_get_compression_tip(hermes_state_db_t *db,
                                       const char *session_id);

/* PoP: resolve_resume_session_id @ hermes_state.py:resolve_resume_session_id
 * Compression-tip first, then walk forward to the descendant with the most
 * recent messages (depth cap 32). Returns malloc'd id (caller frees). */
char *hermes_state_resolve_resume_session_id(hermes_state_db_t *db,
                                             const char *session_id);

/* PoP: get_compression_lineage @ hermes_state.py:get_compression_lineage
 * Compression-only chain root->tip. Returns malloc'd JSON array (caller frees). */
char *hermes_state_get_compression_lineage(hermes_state_db_t *db,
                                           const char *session_id);

/* PoP: get_messages_as_conversation @ hermes_state.py:get_messages_as_conversation
 * Active messages in id order as a JSON array of {role,content} rows. Returns
 * malloc'd JSON (caller frees). include_inactive loads soft-deleted rows. */
char *hermes_state_get_messages_as_conversation(hermes_state_db_t *db,
                                                const char *session_id,
                                                bool include_inactive);

/* ── Token counters ────────────────────────────────────────────────────
 * PoP: update_token_counts @ hermes_state.py:update_token_counts
 * absolute=false -> increment; absolute=true -> set. Ensures the session row
 * exists first (INSERT OR IGNORE). */
bool hermes_state_update_token_counts(hermes_state_db_t *db,
                                      const char *session_id,
                                      long long input_tokens,
                                      long long output_tokens,
                                      long long cache_read_tokens,
                                      long long cache_write_tokens,
                                      long long reasoning_tokens,
                                      long long api_call_count,
                                      bool absolute);

/* ── Aux usage (delegates to the dedicated usage DB surface) ────────────
 * PoP: record_auxiliary_usage @ hermes_state.py:record_auxiliary_usage */
bool hermes_state_record_auxiliary_usage(hermes_state_db_t *db,
                                         const char *session_id,
                                         const char *task,
                                         const char *model,
                                         const char *billing_provider,
                                         const char *billing_base_url,
                                         long long input_tokens,
                                         long long output_tokens,
                                         long long cache_read_tokens,
                                         long long cache_write_tokens,
                                         long long reasoning_tokens,
                                         bool has_estimated_cost,
                                         double estimated_cost_usd);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_HERMES_STATE_DB_H */
