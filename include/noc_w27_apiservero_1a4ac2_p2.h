/*
 * noc_w27_apiservero_1a4ac2_p2.h - C surface for the text-shaping half of
 * gateway/platforms/api_server_openai_routes.py (gate w27_apiservero_1a4ac2_p2).
 *
 * What the routes do to model-visible text before it crosses the wire: the
 * "...[N more chars]" truncation rule, the stored-transcript tool-output cap,
 * the response.completed tool-payload trim, the Responses `message` output
 * item, the post-stream transform tail, and the reasoning-item echo predicate.
 */
#ifndef NOC_W27_APISERVERO_1A4AC2_P2_H
#define NOC_W27_APISERVERO_1A4AC2_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _cap_text(text, keep) -> head plus "...[N more chars]". */
json_t *w27_apiservero_1a4ac2_p2_cap_text(const char *text, long keep);

/* _cap_history_tool_outputs(history, max_chars) -> copy with oversized tool
 * outputs and tool-call argument strings cut down (max_chars <= 0 = verbatim). */
json_t *w27_apiservero_1a4ac2_p2_cap_history_tool_outputs(const json_t *history, long max_chars);

/* _trim_tool_items(items) -> items with large function_call payloads and
 * function_call_output text trimmed for the terminal response.completed. */
json_t *w27_apiservero_1a4ac2_p2_trim_tool_items(const json_t *items);

/* _message_item(text) -> {type:"message", role:"assistant",
 * content:[{type:"output_text", text}]}. */
json_t *w27_apiservero_1a4ac2_p2_message_item(const json_t *text);

/* _post_stream_transform(result) -> [text, appended]. */
json_t *w27_apiservero_1a4ac2_p2_post_stream_transform(const json_t *result);

/* _is_reasoning_input_item(item) -> true for an echoed-back reasoning item. */
json_t *w27_apiservero_1a4ac2_p2_is_reasoning_input_item(const json_t *item);

#ifdef __cplusplus
}
#endif
#endif
