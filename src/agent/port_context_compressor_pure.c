/*
 * port_context_compressor_pure.c — C11 port of pure helpers from
 * agent/context_compressor.py that are NOT yet in
 * context_compressor_pure.c or port_context_compressor_ports.c.
 *
 * Faithful translations of the deterministic, side-effect-free helpers.
 * Reuses libjson (lib/libjson/json.h) for JSON content/parts handling.
 *
 * NOTE: cc_safe_int, cc_skill_pruned_marker, cc_extract_pruned_skill_names,
 * cc_is_image_part, cc_content_has_images, cc_strip_images_from_content,
 * cc_append_text_to_content, cc_strip_image_parts_from_parts, and
 * cc_truncate_tool_call_args_json are already implemented in sibling files.
 * This file contains ONLY the two remaining pure helpers.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "port_context_compressor_pure.h"
#include "libjson/json.h"

#include <stdlib.h>
#include <string.h>

/* ── _fresh_compaction_message_copy ────────────────────────── */
/* PoP: _fresh_compaction_message_copy @ agent/context_compressor.py:_fresh_compaction_message_copy */
json_t *cc_fresh_compaction_message_copy(json_t *msg)
{
    json_t *fresh = json_copy(msg);
    if (fresh) json_obj_del(fresh, "_db_persisted");
    return fresh;
}

/* ── _template_visible_role ─────────────────────────────────── */
/*
 * PoP: _template_visible_role @ agent/context_compressor.py:_template_visible_role
 * Returns NULL for messages the alternation check skips (tool, or
 * assistant with tool_calls). Otherwise returns "assistant"/"user"/etc.
 */
const char *cc_template_visible_role(json_t *message)
{
    if (!message || message->type != JSON_OBJECT) return NULL;
    json_t *role = json_obj_get(message, "role");
    if (!role || role->type != JSON_STRING) return NULL;
    const char *r = role->str_val;
    if (strcmp(r, "tool") == 0) return NULL;
    if (strcmp(r, "assistant") == 0) {
        json_t *tc = json_obj_get(message, "tool_calls");
        if (tc) return NULL;  /* has tool_calls */
    }
    return r;
}
