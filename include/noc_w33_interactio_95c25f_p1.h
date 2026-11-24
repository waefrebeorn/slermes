/*
 * noc_w33_interactio_95c25f_p1.h - C surface for upstream
 * plugins/model-providers/gemini/interactions.py, REQUEST side
 * (gate w33_interactio_95c25f_p1).
 *
 * The wire (``POST {base}/interactions``) is a seam: no HTTP client, no auth
 * header, no retry policy here. This file owns only what values decide -- an
 * OpenAI chat history turned into typed Interactions steps (text blocks, image
 * data URLs, tool results, assistant thought/model_output/function_call steps
 * with replayed thought signatures), and the request body those steps ride in.
 */
#ifndef NOC_W33_INTERACTIO_95C25F_P1_H
#define NOC_W33_INTERACTIO_95C25F_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w33_interactio_95c25f_p1_input_steps @ plugins/model-providers/gemini/interactions.py:build_input_steps
 * messages -> {"steps": [...], "system_instruction": <str|null>}. */
json_t *w33_interactio_95c25f_p1_input_steps(const json_t *messages);

/* PoP: w33_interactio_95c25f_p1_request @ plugins/model-providers/gemini/interactions.py:build_interactions_request
 * spec: {model, messages, tools, tool_choice, temperature, max_tokens, top_p,
 *        stop, thinking_config, response_format} -> the request body. */
json_t *w33_interactio_95c25f_p1_request(const json_t *spec);

/* PoP: w33_interactio_95c25f_p1_continuation @ plugins/model-providers/gemini/interactions.py:continuation_request
 * spec: {request, interaction_id, token} -> request + previous_interaction_id + continuation_token. */
json_t *w33_interactio_95c25f_p1_continuation(const json_t *spec);

/* PoP: w33_interactio_95c25f_p1_tools @ plugins/model-providers/gemini/interactions.py:_tools
 * OpenAI tool list -> [{type, name, description?, parameters?}]. */
json_t *w33_interactio_95c25f_p1_tools(const json_t *tools);

/* PoP: w33_interactio_95c25f_p1_tool_choice @ plugins/model-providers/gemini/interactions.py:_tool_choice
 * "auto"/"required"/"none", or {"function": {"name": ...}} -> the wire choice, else null. */
json_t *w33_interactio_95c25f_p1_tool_choice(const json_t *tool_choice);

/* PoP: w33_interactio_95c25f_p1_response_format @ plugins/model-providers/gemini/interactions.py:_response_format
 * OpenAI response_format -> {"type": "text", "mime_type": "application/json"[, "schema": ...]}. */
json_t *w33_interactio_95c25f_p1_response_format(const json_t *response_format);

/* PoP: w33_interactio_95c25f_p1_image @ plugins/model-providers/gemini/interactions.py:_image_content
 * One image_url item -> {"type": "image", "mime_type", "data"} for a data: URL, else null. */
json_t *w33_interactio_95c25f_p1_image(const json_t *item);

/* PoP: w33_interactio_95c25f_p1_content_blocks @ plugins/model-providers/gemini/interactions.py:_content_blocks
 * OpenAI message content -> [{"type": "text"|"image", ...}]. */
json_t *w33_interactio_95c25f_p1_content_blocks(const json_t *content);

/* PoP: w33_interactio_95c25f_p1_function_result @ plugins/model-providers/gemini/interactions.py:_function_result
 * spec: {message, names} -> one function_result step. */
json_t *w33_interactio_95c25f_p1_function_result(const json_t *spec);

/* PoP: w33_interactio_95c25f_p1_call_arguments @ plugins/model-providers/gemini/interactions.py:_call_arguments
 * A tool call's function.arguments -> a dict (parsed, {"_raw": ...} or {"_value": ...}). */
json_t *w33_interactio_95c25f_p1_call_arguments(const json_t *tool_call);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W33_INTERACTIO_95C25F_P1_H */
