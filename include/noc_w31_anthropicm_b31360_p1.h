/*
 * noc_w31_anthropicm_b31360_p1.h - C surface for upstream
 * agent/anthropic_message_convert.py, gate w31_anthropicm_b31360_p1.
 *
 * The whole outbound message pipeline: system-prompt assembly, per-role
 * message conversion (text / image / tool_use / tool_result blocks), the
 * ordered-block replay carrier, orphaned tool-pair stripping, consecutive-role
 * merging, the leading-user filler, thinking-signature policy and the final
 * blank-block scrub.
 *
 * Seams (not ported here, they are their own modules or I/O):
 *   - agent/image_eviction_policy.py (_evict_old_screenshots): the screenshot
 *     retire budget is a separate policy module; requests below its trigger
 *     (block ceiling / byte budget) come back untouched, which is the shape
 *     these fixtures exercise.
 *   - tools/vision_tools_image_prep.rasterize_svg_data_url: writes temp files
 *     through a soft rasterizer dependency; with no rasterizer installed (the
 *     oracle host) an SVG data URL always takes the placeholder branch.
 */
#ifndef NOC_W31_ANTHROPICM_B31360_P1_H
#define NOC_W31_ANTHROPICM_B31360_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W31_ANTHROPICM_B31360_P1_EMPTY_TEXT "(empty)"
#define W31_ANTHROPICM_B31360_P1_EMPTY_MESSAGE "(empty message)"
#define W31_ANTHROPICM_B31360_P1_NO_OUTPUT "(no output)"
#define W31_ANTHROPICM_B31360_P1_TOOL_CALL_REMOVED "(tool call removed)"
#define W31_ANTHROPICM_B31360_P1_TOOL_RESULT_REMOVED "(tool result removed)"
#define W31_ANTHROPICM_B31360_P1_THINKING_ELIDED "(thinking elided)"

/* messages: OpenAI-shaped [{"role","content",...}] ->
 * {"system": <null|str|blocks>, "messages": [{"role","content"}...]}.
 * base_url / model may be NULL; they select the thinking-signature policy. */
json_t *w31_anthropicm_b31360_p1_convert(const json_t *messages,
                                         const char *base_url,
                                         const char *model);

#ifdef __cplusplus
}
#endif
#endif
