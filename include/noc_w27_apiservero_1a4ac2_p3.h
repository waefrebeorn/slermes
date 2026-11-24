/*
 * noc_w27_apiservero_1a4ac2_p3.h - C surface for the wire-parsing half of
 * gateway/platforms/api_server_openai_routes.py (gate w27_apiservero_1a4ac2_p3).
 *
 * The request payload readers and the Responses envelope writers: Chat
 * `messages` -> (system prompt, turns) with the OpenAI content normalisation
 * and its 400 envelopes, Responses `input` / `conversation_history` readers,
 * and the `_ResponsesStream` value surfaces (envelope, terminal envelope with
 * usage, history-with-user, terminal items).
 */
#ifndef NOC_W27_APISERVERO_1A4AC2_P3_H
#define NOC_W27_APISERVERO_1A4AC2_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _parse_chat_messages(messages) ->
 * {system_prompt, messages, error} where error is null or {status, body}. */
json_t *w27_apiservero_1a4ac2_p3_parse_chat_messages(const json_t *messages);

/* _parse_responses_input(raw_input) -> {messages, error}. */
json_t *w27_apiservero_1a4ac2_p3_parse_responses_input(const json_t *raw_input);

/* _parse_conversation_history(raw_history) -> {history, error}. */
json_t *w27_apiservero_1a4ac2_p3_parse_conversation_history(const json_t *raw_history);

/* _ResponsesStream.envelope(status) ->
 * {id, object:"response", status, created_at, model}. */
json_t *w27_apiservero_1a4ac2_p3_stream_envelope(const char *response_id, double created_at,
                                                 const char *model, const char *status);

/* _ResponsesStream.terminal_envelope(status, output, error=...) ->
 * envelope + output + optional error (+ usage from the turn's usage dict).
 * durable selects the keyed-replay error shape. */
json_t *w27_apiservero_1a4ac2_p3_terminal_envelope(const char *response_id, double created_at,
                                                   const char *model, const char *status,
                                                   const json_t *output, const json_t *error,
                                                   const json_t *usage, int durable);

/* _ResponsesStream._history_with_user() -> history plus this turn's user row. */
json_t *w27_apiservero_1a4ac2_p3_history_with_user(const json_t *history,
                                                   const json_t *user_message);

/* _ResponsesStream._final_items() -> trimmed emitted items plus the final
 * assistant message item. */
json_t *w27_apiservero_1a4ac2_p3_final_items(const json_t *emitted_items,
                                             const json_t *final_response_text,
                                             const json_t *agent_error);

#ifdef __cplusplus
}
#endif
#endif
