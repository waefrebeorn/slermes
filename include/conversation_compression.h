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
/* Opaque: fencing mutex + cancelled/commit_started/commit_phase +
 * admission_revoked + release-guard mutex + monotonic progress. */
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
/* commit_in_flight: lock-free read of the commit-phase marker. */
bool cc_commit_fence_commit_in_flight(cc_commit_fence_t *f);
/* is_cancelled: cancelled || admission_revoked. */
bool cc_commit_fence_is_cancelled(cc_commit_fence_t *f);
/* revoke_commit_admission: lock-free flag; releases durable lease when the
 * fence is free, defers to finish/begin_commit otherwise. */
void cc_commit_fence_revoke_commit_admission(cc_commit_fence_t *f);
/* begin_lock_setup / finish_lock_setup: fence durable-lock acquisition. */
bool cc_commit_fence_begin_lock_setup(cc_commit_fence_t *f);
void cc_commit_fence_finish_lock_setup(cc_commit_fence_t *f);
/* register_cancelled_lock_release: publish holder-qualified release hook;
 * returns whether cleanup was already requested (hook runs synchronously). */
bool cc_commit_fence_register_cancelled_lock_release(cc_commit_fence_t *f,
                                                     void (*release)(void));
/* clear_cancelled_lock_release: forget `release` after normal cleanup. */
void cc_commit_fence_clear_cancelled_lock_release(cc_commit_fence_t *f,
                                                  void (*release)(void));
/* release_cancelled_compression_lock: request + run the cancelled worker's
 * lock release (retained and fulfilled synchronously if hook unpublished). */
void cc_commit_fence_release_cancelled_compression_lock(cc_commit_fence_t *f);

/* ── Bounded compression-pool admission (#76354 F6) ──────────────────── */
/* Reserve one bounded admission slot; false when every pool slot is taken. */
bool cc_try_admit_compression_job(void);
/* Free an admission slot (future done-callback or failed submit). */
void cc_release_compression_admission(void);

/* resolve_context_compression_timeouts: parse the compression config json
 * (may be NULL) into (idle_timeout_seconds, total_ceiling_seconds).
 * Defaults 120.0 / 600.0; ceiling clamped to >= idle when idle > 0. */
void cc_resolve_context_compression_timeouts(const char *compression_cfg_json,
                                             double *out_idle,
                                             double *out_ceiling);

/* ── Compressor attempt-state snapshot/restore ────────────────────── */
/* _snapshot_compressor_attempt_state: copy only allow-listed mutable
 * bookkeeping from the compressor's vars/state dict. Caller frees. */
json_t *cc_snapshot_compressor_attempt_state(const json_t *state);
/* _restore_compressor_attempt_state: restore snapshot into state after a
 * pre-commit hard cancel, with durable cooldown rollback via the SessionDB.
 * durable_cooldown_authoritative=false with cooldown_persist_failed=true
 * triggers a recompute-and-re-record path. */
void cc_restore_compressor_attempt_state(json_t *state,
                                         const json_t *snapshot,
                                         bool durable_cooldown_authoritative,
                                         const json_t *durable_cooldown_state,
                                         hermes_state_db_t *db,
                                         const char *session_id);
/* _capture_authoritative_cooldown_under_lease: refresh + snapshot the built-in
 * durable cooldown state under the session lease. Returns authoritative flag
 * and a fresh durable-state object (caller frees *out_durable_state). */
void cc_capture_authoritative_cooldown_under_lease(json_t *state,
                                                   json_t *attempt_snapshot,
                                                   hermes_state_db_t *db,
                                                   const char *session_id,
                                                   bool *out_authoritative,
                                                   json_t **out_durable_state);

/* ── Compress-timeout executor pool (#76354 F6) ────────────────────── */
/* _get_compress_timeout_executor: process-wide bounded daemon pool (4 workers).
 * The pool type is opaque; use cc_run_compress_context_with_progress_timeout
 * to submit compression jobs through it. */
typedef struct cc_compress_pool cc_compress_pool_t;
cc_compress_pool_t *cc_get_compress_timeout_executor(void);

/* run_compress_context_with_progress_timeout: faithful port of Python's
 * run_compress_context_with_progress_timeout. Runs worker_fn(worker_arg)
 * under a progress-aware idle timeout bounded by a total ceiling. The fence
 * prevents late commits from mutating session state on cancellation.
 * Returns the worker's result string, or fallback_prompt on timeout.
 * fence may be NULL (a transient one is created and destroyed internally). */
char *cc_run_compress_context_with_progress_timeout(
    void (*worker_fn)(void *),
    void *worker_arg,
    const char *fallback_prompt,
    double idle_timeout_seconds,
    double total_ceiling_seconds,
    void (*on_timeout)(double idle, double waited, double since_progress),
    void (*on_commit_overrun)(double waited, double ceiling),
    cc_commit_fence_t *fence);

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

/* ── _CompressionActivityHeartbeat ──────────────────────────────────── */
typedef struct cc_heartbeat cc_heartbeat_t;

cc_heartbeat_t *cc_heartbeat_new(void (*touch_activity)(const char *),
                                  cc_commit_fence_t *fence);
void cc_heartbeat_free(cc_heartbeat_t *h);
/* _fence_cancelled: fence is not None and fence.is_cancelled. */
bool cc_heartbeat_fence_cancelled(const cc_heartbeat_t *h);
/* _should_suppress: latched once a fence-cancel is observed. */
bool cc_heartbeat_should_suppress(cc_heartbeat_t *h);

/* ── Codex app-server compaction route ───────────────────────────────── */

/* Result of a codex-native compaction turn (mirrors the Python TurnResult
 * fields the route consumes: error string, interrupted, should_retire). */
typedef struct cc_codex_compact_result {
    const char *error;        /* NULL = success; malloc'd string owned by caller */
    bool interrupted;
    bool should_retire;
} cc_codex_compact_result_t;

/* Vtable seam the codex transport binds into. NULL entries force the
 * Hermes-native fallback path in cc_compress_context_via_codex_app_server. */
typedef struct cc_codex_session_vtable {
    cc_codex_compact_result_t (*compact_thread)(void *session);
    void (*close)(void *session);
    bool (*has_update_from_response)(void *compressor);
    void (*record_compaction)(void *ctx, cc_codex_compact_result_t *r,
                              long long approx_tokens, bool force);
    void (*record_usage)(void *agent);
    void (*update_from_response)(void *compressor);
} cc_codex_session_vtable_t;

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
