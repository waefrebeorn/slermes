#ifndef PORT_ANTHROPIC_MESSAGE_CONVERT_H
#define PORT_ANTHROPIC_MESSAGE_CONVERT_H

#include "hermes_json.h"

/* Port of Python: agent/anthropic_message_convert.py — the block-shaping
 * helpers that decide what a stored Anthropic block looks like when it is
 * replayed as REQUEST input.
 *
 * SDK response blocks carry output-only siblings the input schema rejects, so
 * each replayer whitelists fields per type. Unknown types are dropped, a blank
 * text block is dropped (the caller relocates its cache_control), and an
 * image only survives with a dict source.
 */

#define AMC_EMPTY_TEXT_PLACEHOLDER "(empty)"

/* Copy `b`'s dict-valued cache_control onto `out` (returned as a new object). */
json_t *amc_carry_cache_control(const char *out_json, const char *block_json,
                                int copy_marker);

/* {"kept": [...], "relocated": <last dropped cache_control or null>,
 *  "dropped": [indexes]} */
json_t *amc_split_blank_text_blocks(const char *blocks_json);

/* OpenAI URL / data URL -> Anthropic image block, or a text placeholder when
 * the inline subtype is one the API rejects. No SVG rasterizer is available in
 * C, so an SVG data URL always lands on the placeholder. */
json_t *amc_image_block_from_openai_url(const char *url);

/* `text` when it has non-whitespace content, else "(empty)". Malloc'd. */
char *amc_safe_text(const char *text);

/* Replay whitelists. NULL means "drop this block". */
json_t *amc_replay_text(const char *block_json);
json_t *amc_replay_thinking(const char *block_json);
json_t *amc_replay_redacted_thinking(const char *block_json);
json_t *amc_replay_tool_use(const char *block_json);
json_t *amc_replay_image(const char *block_json);

/* The dispatch Python uses: whitelist by type, drop unknown types. */
json_t *amc_sanitize_replay_block(const char *block_json);

#endif /* PORT_ANTHROPIC_MESSAGE_CONVERT_H */
