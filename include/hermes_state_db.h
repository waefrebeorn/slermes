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
bool hermes_state_apply_telegram_topic_migration(hermes_state_db_t *db);
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

/* ── Archive / pin / sweep / listing (hermes_state_archive.c) ───────────
 * PoP: set_session_pinned @ hermes_state.py:set_session_pinned
 * Like set_session_archived, flips the WHOLE compression lineage. */
bool hermes_state_set_session_pinned(hermes_state_db_t *db,
                                     const char *session_id, bool pinned);

/* PoP: archive_stale_sessions @ hermes_state.py:archive_stale_sessions
 * Archive lineage tips idle >= idle_days (latest message ts, fallback
 * started_at). Returns count archived; <=0 idle_days is a no-op. */
int hermes_state_archive_stale_sessions(hermes_state_db_t *db,
                                        double idle_days, bool exclude_pinned);

/* PoP: list_prune_candidates @ hermes_state.py:list_prune_candidates
 * Dry-run listing (oldest-first by last_active). archived_filter: -1 any,
 * 0 unarchived only, 1 archived only. Returns malloc'd JSON array. */
char *hermes_state_list_prune_candidates(hermes_state_db_t *db,
                                         double older_than_days,
                                         const char *source,
                                         bool require_ended,
                                         int archived_filter);

/* PoP: archive_sessions @ hermes_state.py:archive_sessions
 * Bulk-archive matches (whole lineage each). Returns matches count. */
int hermes_state_archive_sessions(hermes_state_db_t *db,
                                  double older_than_days, const char *source);

/* PoP: list_sessions_rich @ hermes_state.py:list_sessions_rich
 * last_active DESC ordering; hides archived (or shows only archived) and
 * compression-away roots. Returns malloc'd JSON array. */
char *hermes_state_list_sessions_rich(hermes_state_db_t *db,
                                      bool archived_only);

/* ── Alternation repair (hermes_state_repair.c) ─────────────────────────
 * Flat message record for repair_message_sequence. Strings are owned
 * (malloc'd) by the caller; the repair frees strings of merged/dropped
 * rows and compacts the array in place.
 *   tool_call_ids: for assistant rows, ";"-separated "id[,call_id]" entries
 *                  describing the turn's tool_calls (superset match #58168).
 *   tool_call_id:  for tool rows, the id the result answers.
 *   codex_items:   true when the row carries codex reasoning/message items
 *                  (codex interims are exempt from Pass 0 merging). */
typedef struct {
    char *role;               /* "user" | "assistant" | "tool" | ... */
    char *content;            /* plain-text content or NULL */
    char *tool_call_id;       /* tool rows */
    char *tool_call_ids;      /* assistant rows: ";"-joined id[,call_id] */
    char *finish_reason;      /* "incomplete", "verification_required", ... */
    char *reasoning_content;  /* thinking-provider reasoning or NULL */
    bool  codex_items;        /* codex_reasoning_items/codex_message_items */
} repair_msg_t;

/* PoP: repair_message_sequence @ agent/agent_runtime_helpers.py:repair_message_sequence
 * Three passes (merge assistant runs / drop orphan+duplicate tool results /
 * merge user runs). Compacts msgs in place, updates *count, returns number
 * of repairs. */
int hermes_state_repair_message_sequence(repair_msg_t *msgs, int *count);

/* ================================================================
 *  Compression locks + child publication (hermes_state_locks.c)
 * ================================================================ */

/* PoP: hermes_state_lock_holder_process_is_dead @ hermes_state.py:_compression_lock_holder_process_is_dead
 * True only when a structured "pid=<n>:..." holder's local PID is provably
 * gone (kill(pid,0) → ESRCH). Same-process / unstructured / doubt → false. */
bool hermes_state_lock_holder_process_is_dead(const char *holder);

/* PoP: hermes_state_try_acquire_compression_lock @ hermes_state.py:try_acquire_compression_lock
 * Atomic DELETE-expired/dead + INSERT OR IGNORE + SELECT-confirm inside one
 * BEGIN IMMEDIATE. True = caller owns the lock. */
bool hermes_state_try_acquire_compression_lock(hermes_state_db_t *db,
                                               const char *session_id,
                                               const char *holder,
                                               double ttl_seconds);

/* PoP: hermes_state_release_compression_lock @ hermes_state.py:release_compression_lock
 * Idempotent holder-checked DELETE. */
void hermes_state_release_compression_lock(hermes_state_db_t *db,
                                           const char *session_id,
                                           const char *holder);

/* PoP: hermes_state_refresh_compression_lock @ hermes_state.py:refresh_compression_lock
 * Extend lease iff holder still owns the row (holder column alone — a
 * starved-but-live owner past its own TTL must be able to revive). */
bool hermes_state_refresh_compression_lock(hermes_state_db_t *db,
                                           const char *session_id,
                                           const char *holder,
                                           double ttl_seconds);

/* PoP: hermes_state_get_compression_lock_holder @ hermes_state.py:get_compression_lock_holder
 * Current non-expired holder (malloc'd) or NULL. */
char *hermes_state_get_compression_lock_holder(hermes_state_db_t *db,
                                               const char *session_id);

/* PoP: hermes_state_find_live_compression_child @ hermes_state.py:find_live_compression_child
 * Unique live direct child id (malloc'd) of a compression-ended parent, or
 * NULL when the parent is live / reason differs / 0 or 2+ candidates. */
char *hermes_state_find_live_compression_child(hermes_state_db_t *db,
                                               const char *parent_session_id);

/* PoP: hermes_state_publish_compression_child @ hermes_state.py:publish_compression_child
 * One transaction: lease check, child row inheriting parent routing/origin
 * columns, compacted handoff insert, parent close (end_reason='compression').
 * Returns 0 on success; negative code (-2 lease lost, -3 parent missing,
 * -4 parent already ended, -5 empty handoff, -6/-7/-8 SQL) on rollback. */
int hermes_state_publish_compression_child(hermes_state_db_t *db,
                                           const char *parent_session_id,
                                           const char *child_session_id,
                                           const char *source,
                                           const char *messages_json,
                                           const char *model,
                                           const char *model_config_json,
                                           const char *system_prompt,
                                           const char *cwd,
                                           const char *profile_name,
                                           const char *compression_lock_holder,
                                           bool require_compression_lease);

#ifdef __cplusplus
}
#endif

/* ── Compression failure cooldown rows ────────────────────────────────── */
/* These live in hermes_state_misc.c but are declared here so the
 * conversation_compression port can call them through the opaque handle. */
char *hermes_state_get_compression_failure_cooldown_row(hermes_state_db_t *db,
                                                        const char *session_id);
int hermes_state_restore_compression_failure_cooldown_row(hermes_state_db_t *db,
                                                          const char *session_id,
                                                          const char *snapshot_json);
void hermes_state_record_compression_failure_cooldown(hermes_state_db_t *db,
                                                      const char *session_id,
                                                      double cooldown_until,
                                                      const char *error);
void hermes_state_clear_compression_failure_cooldown(hermes_state_db_t *db,
                                                     const char *session_id);

#endif /* SLERMES_HERMES_STATE_DB_H */
