/*
 * compaction_display.c — Faithful C11 port of agent/compaction_display.py.
 *
 * Client-facing projection helpers for model-only compaction carriers.
 *
 * Model-facing recovery history retains the complete carrier. Display
 * projections instead remove the handoff, inherited tool state, and internal
 * reasoning while preserving any real prior-tail content or live user ask
 * embedded in the carrier.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "context_compressor_pure.h"
#include "compaction_display.h"

/* Python module constant: internal model-facing fields stripped for display. */
static const char *const COMPACTION_INTERNAL_FIELDS[] = {
    "tool_calls",
    "finish_reason",
    "reasoning",
    "reasoning_content",
    "reasoning_details",
    "codex_reasoning_items",
    "codex_message_items",
};
#define N_COMPACTION_INTERNAL_FIELDS \
    (sizeof(COMPACTION_INTERNAL_FIELDS) / sizeof(COMPACTION_INTERNAL_FIELDS[0]))

/* PoP: project_compaction_message_for_display @ agent/compaction_display.py:project_compaction_message_for_display */
json_t *project_compaction_message_for_display(const json_t *message)
{
    if (!message || message->type != JSON_OBJECT)
        return NULL; /* not isinstance(message, dict) */

    if (!cc_is_context_summary_message(message)) {
        /* not a compaction summary → return message.copy() */
        return json_copy(message);
    }

    /* ContextCompressor._strip_context_summary_handoff_message(message);
     * NULL ⇒ pure handoff with nothing authentic to display. The strip
     * helper already returns a freshly allocated copy we may mutate. */
    json_t *projected = cc_strip_context_summary_handoff_message(message);
    if (!projected)
        return NULL;

    for (size_t i = 0; i < N_COMPACTION_INTERNAL_FIELDS; i++)
        json_obj_del(projected, COMPACTION_INTERNAL_FIELDS[i]);
    json_obj_del(projected, "display_kind");
    return projected;
}
