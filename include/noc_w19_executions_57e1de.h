#ifndef NOC_W19_EXECUTIONS_57E1DE_H
#define NOC_W19_EXECUTIONS_57E1DE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_executions_57e1de_key_set @ persistence/sqlite/workers/executions.py:_key_set
 * A keyword payload's key set, checked before the store: "ok", or "invalid_params" when a
 * key is outside `allowed` or a `required` key is missing. `keys` may be the payload object
 * or the list of its key names. */
struct json_t *w19_executions_57e1de_key_set(struct json_t *keys, struct json_t *allowed,
                                             struct json_t *required);

/* PoP: w19_executions_57e1de_messages @ persistence/sqlite/workers/executions.py:_messages
 * Each mapping message becomes a plain dict; any other item is passed through. */
struct json_t *w19_executions_57e1de_messages(struct json_t *messages);

/* PoP: w19_executions_57e1de_record @ persistence/sqlite/workers/executions.py:_record
 * The stored row -> WorkerExecutionRecord: id/kind/epoch/generation/status/sequence, with
 * WorkerKind/WorkerStatus membership and `last_sequence >= 0` (ValueError otherwise). */
struct json_t *w19_executions_57e1de_record(struct json_t *public_fields);

/* PoP: w19_executions_57e1de_payload_usage_main @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.record_usage
 * `{"payload": dict(usage)}` after the WorkerMainUsage key-set check (operation usage.main). */
struct json_t *w19_executions_57e1de_payload_usage_main(struct json_t *usage,
                                                         struct json_t *allowed);

/* PoP: w19_executions_57e1de_payload_usage_auxiliary @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.record_auxiliary_usage
 * `dict(usage, task=task)` after the WorkerAuxiliaryUsage key-set check. */
struct json_t *w19_executions_57e1de_payload_usage_auxiliary(struct json_t *usage, const char *task,
                                                             struct json_t *allowed);

/* PoP: w19_executions_57e1de_payload_create @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.create_session
 * `dict(fields)` after WorkerSessionCreate's key check, whose `source` is required. */
struct json_t *w19_executions_57e1de_payload_create(struct json_t *fields, struct json_t *allowed);

/* PoP: w19_executions_57e1de_payload_append @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.append_transcript
 * `{"messages": _messages(messages), **options}` after the transcript-option key check. */
struct json_t *w19_executions_57e1de_payload_append(struct json_t *messages, struct json_t *options,
                                                    struct json_t *allowed);

/* PoP: w19_executions_57e1de_payload_user_content @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.set_user_message_content
 * `{"row_id","content"}` plus `display_metadata` only when one was passed (never a null key). */
struct json_t *w19_executions_57e1de_payload_user_content(struct json_t *row_id, struct json_t *content,
                                                          struct json_t *display_metadata,
                                                          int has_display_metadata);

/* PoP: w19_executions_57e1de_payload_restore_cooldown @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.restore_compression_cooldown
 * `{"snapshot": {"session_exists","cooldown_until","error"}}`. */
struct json_t *w19_executions_57e1de_payload_restore_cooldown(struct json_t *snapshot);

/* PoP: w19_executions_57e1de_payload_lease @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.acquire_turn_lease
 * `{"holder","ttl_seconds"}` (the turn-lease and compression-lock payload). */
struct json_t *w19_executions_57e1de_payload_lease(const char *holder, struct json_t *ttl_seconds);

/* PoP: w19_executions_57e1de_payload_boundary @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.compression_boundary
 * `{"session_key","source"}` from a ConversationPeer. */
struct json_t *w19_executions_57e1de_payload_boundary(struct json_t *peer);

/* PoP: w19_executions_57e1de_payload_activity @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.touch_session_activity
 * `{"ts","description","provenance"}` from a WorkerActivity. */
struct json_t *w19_executions_57e1de_payload_activity(struct json_t *activity);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_EXECUTIONS_57E1DE_H */
