#ifndef PORT_CONTEXT_COMPRESSOR_PURE_H
#define PORT_CONTEXT_COMPRESSOR_PURE_H

/* C11 port of pure helpers from agent/context_compressor.py.
 *
 * This header declares ONLY the functions implemented in
 * src/agent/port_context_compressor_pure.c:
 *   cc_fresh_compaction_message_copy, cc_template_visible_role
 *
 * The other pure helpers of context_compressor.py live in sibling files:
 *   - src/agent/context_compressor_pure.c      -> include/context_compressor_pure.h
 *   - src/agent/port_context_compressor_ports.c-> include/port_context_compressor_ports.h
 * Include those headers for cc_safe_int, cc_skill_pruned_marker,
 * cc_extract_pruned_skill_names, cc_is_image_part, cc_content_has_images,
 * cc_strip_images_from_content, cc_append_text_to_content,
 * cc_strip_image_parts_from_parts, cc_truncate_tool_call_args_json.
 */

#include <stdbool.h>
#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── _fresh_compaction_message_copy ────────────────────────── */
/* Deep-copy a message dict, stripping _db_persisted. Caller owns result. */
json_t *cc_fresh_compaction_message_copy(json_t *msg);

/* ── _template_visible_role ──────────────────────────────────── */
/* Returns role string, or NULL for messages the alternation check skips. */
const char *cc_template_visible_role(json_t *message);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CONTEXT_COMPRESSOR_PURE_H */
