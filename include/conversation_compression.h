/* conversation_compression.h — faithful C11 port of the module-level
 * helpers of agent/conversation_compression.py: the commit fence, the
 * lock-skip signal, rotation recovery, compaction message shaping
 * (_message_text, _is_real_user_message, anchor insertion/merging),
 * context-engine notification staging, and compression telemetry.
 *
 * Reuses (does not duplicate): context.c's context_compressor__* summary
 * classifiers and hermes_state_locks.c's compression-lock surface.
 */
#ifndef SLERMES_CONVERSATION_COMPRESSION_H
#define SLERMES_CONVERSATION_COMPRESSION_H

#include <stdbool.h>
#include "json.h"
#include "hermes_state_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Status constants (module top) ───────────────────────────────────── */
extern const char *CC_COMPACTION_DONE_STATUS;
extern const char *CC_TODO_INJECTION_HEADER;
extern const char *CC_CONTINUATION_USER_CONTENT;
extern const char *CC_LEGACY_CONTINUATION_USER_CONTENT;

/* ── CompressionCommitFence ──────────────────────────────────────────── */
/* Opaque: pthread mutex + cancelled/commit_started + monotonic progress. */
typedef struct cc_commit_fence cc_commit_fence_t;

cc_commit_fence_t *cc_commit_fence_new(void);
void cc_commit_fence_free(cc_commit_fence_t *f);
void cc_commit_fence_touch_progress(cc_commit_fence_t *f);
double cc_commit_fence_seconds_since_progress(cc_commit_fence_t *f);
/* true = cancellation won before the commit boundary. */
bool cc_commit_fence_cancel_before_commit(cc_commit_fence_t *f);
/* 1 = cancelled, 0 = commit already started, -1 = fence busy (would block). */
int cc_commit_fence_try_cancel_before_commit(cc_commit_fence_t *f);
/* true = entered the commit boundary; false = cancellation already won. */
bool cc_commit_fence_begin_commit(cc_commit_fence_t *f);
void cc_commit_fence_finish_commit(cc_commit_fence_t *f);

/* ── Lock-skip signal (#69870) ───────────────────────────────────────── */
/* Type-pinned read: skipped iff flag set (holder may be NULL = bare True). */
typedef struct {
    bool skipped;      /* _compression_skipped_due_to_lock is True/str */
    char *holder;      /* confirmed holder string or NULL */
} cc_lock_skip_signal_t;

bool cc_compression_skipped_due_to_lock(const cc_lock_skip_signal_t *sig);

/* ── Rotation recovery ───────────────────────────────────────────────── */
bool cc_session_was_rotated_by_compression(hermes_state_db_t *db,
                                           const char *session_id);

/* _adopt_live_compression_child: resolve→load→revalidate→mutate ordering.
 * On success returns the recovered conversation (json array, caller frees)
 * and writes the adopted child id to *out_child_id (caller frees). */
json_t *cc_adopt_live_compression_child(hermes_state_db_t *db,
                                        const char *parent_session_id,
                                        char **out_child_id);

/* recover_rotated_compression_session: rotation check + bounded holder wait
 * (21 attempts × 50ms while the parent lease is held). */
json_t *cc_recover_rotated_compression_session(hermes_state_db_t *db,
                                               const char *session_id,
                                               char **out_child_id);

/* ── Compaction message shaping ──────────────────────────────────────── */
/* _message_text: str content → itself; list → "\n".join(text|content parts);
 * else "". Caller frees. */
char *cc_message_text(const json_t *message);

/* _is_real_user_message: role=user, no synthetic flags, non-empty text,
 * no synthetic prefixes, not a synthetic compression user turn. */
bool cc_is_real_user_message(const json_t *message);

/* _strip_stale_todo_snapshot: returns a NEW content node (caller frees). */
json_t *cc_strip_stale_todo_snapshot(const json_t *content);

/* _merge_anchor_into_user_message: mutates target in place. */
void cc_merge_anchor_into_user_message(json_t *target, const json_t *anchor);

/* _insert_real_user_anchor: mutates messages array; takes ownership of
 * anchor on every path. */
void cc_insert_real_user_anchor(json_t *messages, json_t *anchor);

/* _ensure_compressed_has_user_turn: preserves human intent post-compaction. */
void cc_ensure_compressed_has_user_turn(const json_t *original_messages,
                                        json_t *compressed);

/* ── Context-engine notification staging ─────────────────────────────── */
typedef bool (*cc_notify_fn)(void *ctx, const char *new_session_id,
                             const char *old_session_id);
typedef struct cc_pending_notification cc_pending_notification_t;

/* _queue_context_engine_compression_notification: stage exactly one; a
 * second queue while one is pending returns NULL (Python raises). */
cc_pending_notification_t *cc_queue_compression_notification(
    cc_pending_notification_t **slot, cc_notify_fn fn, void *ctx,
    const char *new_session_id, const char *old_session_id);

/* finalize_context_engine_compression_notification: emit-or-discard;
 * repeated calls are no-ops (slot cleared first). */
bool cc_finalize_compression_notification(cc_pending_notification_t **slot,
                                          bool committed);

/* ── Telemetry ───────────────────────────────────────────────────────── */
/* _emit_compression_attempt_telemetry: merge base (optional) with the
 * mandated keys, sorted-key compact JSON line (caller frees). */
char *cc_compression_attempt_telemetry_line(const json_t *base_telemetry,
                                            const char *attempt_id,
                                            const char *session_id,
                                            long long total_duration_ms,
                                            const char *commit_status,
                                            const char *split_status,
                                            const char *failure_class,
                                            bool fallback_used);

/* ── Status edge ─────────────────────────────────────────────────────── */
typedef void (*cc_status_cb)(void *ctx, const char *kind, const char *text);
void cc_emit_compaction_done(cc_status_cb cb, void *ctx);

/* ── Skew / guard / kwargs helpers ───────────────────────────────────── */
bool cc_lock_api_is_absent_on_session_db(const void *lock_db);
void cc_refresh_persisted_compression_guards(const void *compressor);
json_t *cc_supported_compression_kwargs(bool has_memory_context,
                                        const char *memory_context,
                                        long long current_tokens,
                                        const char *focus_topic,
                                        bool force);
void cc_activity_heartbeat_touch(void (*touch_activity)(const char *desc),
                                 const char *desc);

/* ── Codex app-server compaction route ───────────────────────────────── */
typedef struct cc_codex_compact_result cc_codex_compact_result_t;
typedef struct cc_codex_session_vtable cc_codex_session_vtable_t;
typedef struct cc_codex_session_ctx cc_codex_session_ctx_t;
json_t *cc_compress_context_via_codex_app_server(
    json_t *messages, const char *system_message,
    cc_codex_session_ctx_t *codex, long long approx_tokens,
    int task_id_is_default, bool force);

extern const char *CC_COMPACTION_STATUS;

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_CONVERSATION_COMPRESSION_H */
