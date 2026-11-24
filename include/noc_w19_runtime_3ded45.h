#ifndef NOC_W19_RUNTIME_3DED45_H
#define NOC_W19_RUNTIME_3DED45_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Chain kinds for w19_runtime_3ded45_derive. */
#define W19_RUNTIME_3DED45_LOCAL 0
#define W19_RUNTIME_3DED45_API   1
#define W19_RUNTIME_3DED45_ROOM  2

/* PoP: w19_runtime_3ded45_derive @ persistence/sqlite/authority/runtime.py:_derive
 * The id of generation ``generation`` of a ``GenerationChain``: the contract package's
 * pure derivations (``api_generation_id`` / ``room_session_id`` /
 * ``local_session_id`` over ``generation_request_id``).  ``identity`` is the room
 * chain's 4-element array.  Returns a new string (caller frees). */
char *w19_runtime_3ded45_derive(int kind, const char *session_id, const struct json_t *identity,
                               const char *profile_id, const char *principal_id,
                               const char *base_request_id, long long generation);

/* PoP: w19_runtime_3ded45_mutation_call @ persistence/sqlite/authority/runtime.py:_mutation_call
 * ``{"operation": getattr(command, "operation", None), "payload": payload() if callable(payload) else None}``.
 * ``operation`` NULL means the attribute is absent (None); a payload that is not
 * callable contributes None whatever it holds. */
struct json_t *w19_runtime_3ded45_mutation_call(const char *operation, const struct json_t *payload,
                                               int payload_is_callable);

/* PoP: w19_runtime_3ded45_legacy_prepared @ persistence/sqlite/authority/runtime.py:_legacy_prepared
 * ``None`` stays null; otherwise ``{"snapshot": {"receipt", "target", "target_revision"}, **values}``. */
struct json_t *w19_runtime_3ded45_legacy_prepared(int is_none, const struct json_t *receipt,
                                                 const char *target, long long target_revision,
                                                 const struct json_t *values);

/* PoP: w19_runtime_3ded45_chat_params @ persistence/sqlite/authority/runtime.py:_chat_params
 * ``{"request_id", "operation", "payload": dict(payload), "expected_revision", "expected_generation"}``. */
struct json_t *w19_runtime_3ded45_chat_params(const char *request_id, const char *operation,
                                             const struct json_t *payload, long long expected_revision,
                                             long long expected_generation);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_RUNTIME_3DED45_H */
