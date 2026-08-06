/*
 * port_agent_relay_llm.h — C11 port of agent/relay_llm.py.
 *
 * Provides Relay-managed LLM execution (execute / stream) plus the pure
 * helpers used to shape provider requests: _jsonable, _json_equal, _namespace,
 * _is_cancellation, _provider_request, _relay_request_body, _codec_round_trip,
 * _codec, _provider_request_body, _restore_provider_message_extensions,
 * _logical_parent, _complete_logical, _recover_successful_callback,
 * complete_logical_call, and the ManagedLlmStream / AnthropicStreamAccumulator
 * types.
 */
#ifndef PORT_AGENT_RELAY_LLM_H
#define PORT_AGENT_RELAY_LLM_H

#include <stdbool.h>
#include <stddef.h>
#include "port_agent_relay_runtime.h"
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Pure helpers (no Relay dependency) ────────────────────────────────── */

/* PoP: _jsonable @ agent/relay_llm.py:_jsonable */
/* Returns a malloc'd json_t (caller frees with json_free). */
json_t *relay_llm_jsonable(const void *value);

/* PoP: _namespace @ agent/relay_llm.py:_namespace */
/* Returns a shallow namespace struct mirroring the json keys of `value`.
 * Caller frees via relay_llm_namespace_free. */
typedef struct {
    char  **keys;   /* N keys           */
    void  **vals;   /* N json_t* (owned) */
    size_t  count;
} relay_llm_namespace_t;

relay_llm_namespace_t *relay_llm_namespace(const json_t *value);
void relay_llm_namespace_free(relay_llm_namespace_t *ns);
const void *relay_llm_namespace_get(const relay_llm_namespace_t *ns, const char *key);
const char *relay_llm_namespace_get_str(const relay_llm_namespace_t *ns, const char *key, const char *def);

/* PoP: _json_equal @ agent/relay_llm.py:_json_equal */
bool relay_llm_json_equal(const void *left, const void *right);

/* PoP: _is_cancellation @ agent/relay_llm.py:_is_cancellation */
bool relay_llm_is_cancellation(const char *error_kind);

/* ── Request-shaping helpers ──────────────────────────────────────────── */

/* PoP: _provider_request @ agent/relay_llm.py:_provider_request */
/* Rewrites a serialized provider request JSON by substituting next_request's
 * fields. Returns a malloc'd json_t (caller frees). */
json_t *relay_llm_provider_request(const json_t *original,
                                   const json_t *next_request,
                                   const json_t *relay_request_body,
                                   const json_t *codec_baseline_body,
                                   const json_t *metadata);

/* PoP: _relay_request_body @ agent/relay_llm.py:_relay_request_body */
json_t *relay_llm_relay_request_body(const json_t *request, const json_t *metadata);

/* PoP: _restore_provider_message_extensions @ agent/relay_llm.py:_restore_provider_message_extensions */
json_t *relay_llm_restore_provider_message_extensions(const json_t *provider_response,
                                                      const json_t *relay_request_body,
                                                      const json_t *codec_baseline_body);

/* PoP: _codec_round_trip_request_body @ agent/relay_llm.py:_codec_round_trip_request_body */
json_t *relay_llm_codec_round_trip_request_body(const json_t *backend_codec,
                                                const json_t *relay_request,
                                                const json_t *relay_request_body,
                                                const json_t *metadata);

/* PoP: _codec @ agent/relay_llm.py:_codec */
/* Returns the codec name string (e.g. "OpenAIChatCodec") or NULL.
 * `codec_out` receives a malloc'd name when non-NULL. */
char *relay_llm_codec(const json_t *metadata);

/* PoP: _provider_request_body @ agent/relay_llm.py:_provider_request_body */
json_t *relay_llm_provider_request_body(const json_t *content, const json_t *metadata);

/* ── Logical-call management ────────────────────────────────────────────── */

/* PoP: _logical_parent @ agent/relay_llm.py:_logical_parent */
/* Returns true when a logical scope was found/created on `turn`.
 * Sets *out_handle and *out_request_id (both caller-freed). */
bool relay_llm_logical_parent(relay_runtime_t *runtime,
                              relay_session_t *session,
                              relay_handle_t parent,
                              const json_t *metadata,
                              relay_turn_t **out_turn,
                              relay_handle_t *out_handle,
                              char **out_request_id);

/* PoP: _complete_logical @ agent/relay_llm.py:_complete_logical */
void relay_llm_complete_logical(relay_turn_t *turn,
                                relay_handle_t handle,
                                const char *request_id,
                                const char *outcome);

/* PoP: _recover_successful_callback @ agent/relay_llm.py:_recover_successful_callback */
bool relay_llm_recover_successful_callback(json_t **raw_response,
                                           const char *relay_error_kind,
                                           const char *relay_error_message,
                                           const char *callback_error_kind,
                                           const char *callback_error_message,
                                           relay_turn_t *logical_turn,
                                           relay_handle_t logical_handle,
                                           const char *logical_request_id,
                                           bool defer_logical_completion);

/* PoP: complete_logical_call @ agent/relay_llm.py:complete_logical_call */
void relay_llm_complete_logical_call(const char *api_request_id, const char *outcome);

/* ── Managed LLM execution (Relay path) ──────────────────────────────── */

/* Callback that the provider invokes to produce a raw (non-managed) response.
 * Returns a malloc'd json_t (the serialized provider response). */
typedef json_t *(*relay_llm_provider_callback)(void *user);

/* PoP: execute @ agent/relay_llm.py:execute */
/* Runs one non-streaming provider attempt.
 *   request_json   — serialised provider request (malloc'd, caller frees)
 *   callback       — provider callback
 *   cb_user        — opaque state passed to callback
 *   session_id     — execution session
 *   name           — provider/scope name
 *   model_name     — model identifier
 *   metadata_json  — optional metadata JSON object string (may be NULL)
 *   defer_logical_completion — if true, caller must complete the logical scope
 * Returns a malloc'd json_t (caller frees) or NULL on managed-execution failure. */
json_t *relay_llm_execute(const json_t *request_json,
                          relay_llm_provider_callback callback, void *cb_user,
                          const char *session_id,
                          const char *name, const char *model_name,
                          const char *metadata_json,
                          bool defer_logical_completion);

/* PoP: stream_current @ agent/relay_llm.py:stream_current */
/* Returns a malloc'd json_t (caller frees) or NULL. */
json_t *relay_llm_stream_current(const json_t *request_json,
                                 void *(*stream_factory)(void *user), void *sf_user,
                                 const char *name, const char *model_name,
                                 const char *metadata_json);

/* ── ManagedLlmStream ──────────────────────────────────────────────────── */

typedef struct relay_llm_managed_stream relay_llm_managed_stream_t;

/* PoP: ManagedLlmStream.__init__ @ agent/relay_llm.py:ManagedLlmStream.__init__ */
relay_llm_managed_stream_t *relay_llm_managed_stream_new(
    const json_t *request,
    void *(*stream_factory)(void *user), void *sf_user,
    const char *session_id, const char *name, const char *model_name,
    void (*finalizer)(void *user), void *fin_user,
    void (*on_stream_created)(void *user, void *raw_stream), void *osc_user,
    void (*on_chunk)(void *user, const json_t *chunk), void *oc_user,
                    void *(*chunk_adapter)(const json_t *chunk),
    bool (*accept_chunk)(const json_t *chunk),
    bool (*completed_response_predicate)(const json_t *raw_stream),
    const char *metadata_json,
    bool defer_logical_completion);

void relay_llm_managed_stream_free(relay_llm_managed_stream_t *s);

/* PoP: ManagedLlmStream.__next__ @ agent/relay_llm.py:ManagedLlmStream.__next__ */
/* Returns true + malloc'd json_t when a chunk is available, false on end. */
bool relay_llm_managed_stream_next(relay_llm_managed_stream_t *s, json_t **out_chunk);

/* PoP: ManagedLlmStream.close @ agent/relay_llm.py:ManagedLlmStream.close */
void relay_llm_managed_stream_close(relay_llm_managed_stream_t *s);

/* ── AnthropicStreamAccumulator ────────────────────────────────────────── */

typedef struct anthropic_stream_accumulator anthropic_stream_accumulator_t;

/* PoP: AnthropicStreamAccumulator.__init__ @ agent/relay_llm.py:AnthropicStreamAccumulator.__init__ */
anthropic_stream_accumulator_t *anthropic_stream_accumulator_new(void);
void anthropic_stream_accumulator_free(anthropic_stream_accumulator_t *acc);

/* PoP: AnthropicStreamAccumulator.observe @ agent/relay_llm.py:AnthropicStreamAccumulator.observe */
void anthropic_stream_accumulator_observe(anthropic_stream_accumulator_t *acc, const json_t *event);

/* PoP: AnthropicStreamAccumulator.finalize @ agent/relay_llm.py:AnthropicStreamAccumulator.finalize */
json_t *anthropic_stream_accumulator_finalize(const anthropic_stream_accumulator_t *acc);

/* PoP: AnthropicStreamAccumulator.response @ agent/relay_llm.py:AnthropicStreamAccumulator.response */
relay_llm_namespace_t *anthropic_stream_accumulator_response(const anthropic_stream_accumulator_t *acc,
                                                             const json_t *base);

#ifdef __cplusplus
}
#endif
#endif /* PORT_AGENT_RELAY_LLM_H */
