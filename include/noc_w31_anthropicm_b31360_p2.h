/*
 * noc_w31_anthropicm_b31360_p2.h - C surface for upstream
 * agent/anthropic_message_convert.py, gate w31_anthropicm_b31360_p2.
 *
 * The pure leaf surface around the pipeline: model-id normalization, tool
 * schema conversion, one-content-part (text / image / passthrough) conversion,
 * the replay whitelists, blank-block splitting, coercion helpers, tool-result
 * content resolution, and the thinking-replay predicates the pipeline routes
 * on (endpoint route, model capability, foreign-minted reasoning).
 *
 * Seams:
 *   - tools/vision_tools_image_prep.rasterize_svg_data_url (temp-file soft
 *     dependency; no rasterizer here, so SVG always takes the placeholder).
 *   - agent/bedrock_adapter._resolve_inference_profile_model_id (network);
 *     an unresolved application-profile ARN is "unknown issuer", i.e. it does
 *     NOT count as minted outside Claude.
 */
#ifndef NOC_W31_ANTHROPICM_B31360_P2_H
#define NOC_W31_ANTHROPICM_B31360_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* normalize_model_name(model, preserve_dots) */
json_t *w31_anthropicm_b31360_p2_model_name(const char *model, int preserve_dots);

/* convert_tools_to_anthropic(tools) */
json_t *w31_anthropicm_b31360_p2_convert_tools(const json_t *tools);

/* _normalize_tool_input_schema(schema) */
json_t *w31_anthropicm_b31360_p2_normalize_schema(const json_t *schema);

/* _convert_content_part_to_anthropic(part) -> block or null */
json_t *w31_anthropicm_b31360_p2_content_part(const json_t *part);

/* _convert_content_to_anthropic(content) -> list (non-lists pass through) */
json_t *w31_anthropicm_b31360_p2_content_list(const json_t *content);

/* _content_parts_to_anthropic_blocks(parts) -> tool_result inner blocks */
json_t *w31_anthropicm_b31360_p2_content_parts_blocks(const json_t *parts);

/* _image_block_from_openai_url(url) */
json_t *w31_anthropicm_b31360_p2_image_block(const char *url);

/* _sanitize_replay_block(block) -> whitelisted block or null */
json_t *w31_anthropicm_b31360_p2_sanitize_replay(const json_t *block);

/* _split_blank_text_blocks(blocks) -> {"kept","relocated","dropped"} */
json_t *w31_anthropicm_b31360_p2_split_blank(const json_t *blocks);

/* _is_blank_text_block(block) */
json_t *w31_anthropicm_b31360_p2_is_blank(const json_t *block);

/* _safe_text(text) */
json_t *w31_anthropicm_b31360_p2_safe_text(const json_t *text);

/* _sanitize_tool_id(id) */
json_t *w31_anthropicm_b31360_p2_sanitize_tool_id(const json_t *id);

/* _parse_tool_args(raw) */
json_t *w31_anthropicm_b31360_p2_parse_tool_args(const json_t *raw);

/* _tool_result_content(message) */
json_t *w31_anthropicm_b31360_p2_tool_result_content(const json_t *message);

/* anthropic_thinking_route(base_url, model) */
json_t *w31_anthropicm_b31360_p2_route(const char *base_url, const char *model);

/* route_replays_prior_thinking(route, model) */
json_t *w31_anthropicm_b31360_p2_replays_prior(const char *route, const char *model);

/* bedrock_adapter.reasoning_minted_outside_claude(message) */
json_t *w31_anthropicm_b31360_p2_issuer_foreign(const json_t *message);

#ifdef __cplusplus
}
#endif
#endif
