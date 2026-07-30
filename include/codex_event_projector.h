/*
 * codex_event_projector.h — Projects codex app-server events into Hermes messages.
 *
 * Converts Codex item/* notifications into OpenAI-shaped
 * {role, content, tool_calls, tool_call_id} entries for Hermes' messages list.
 *
 * Maps to Python agent/transports/codex_event_projector.py (312 lines).
 */

#ifndef CODEX_EVENT_PROJECTOR_H
#define CODEX_EVENT_PROJECTOR_H

#include "hermes_core_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Projection result from one codex event */
typedef struct {
    json_node_t **messages;     /* Array of message objects (role, content, etc.) */
    int           msg_count;
    int           msg_capacity;
    bool          is_tool_iteration;
    char         *final_text;   /* Set when agentMessage completes */
} codex_projection_t;

/* Opaque projector handle */
typedef struct codex_projector_t codex_projector_t;

/* Create/destroy projector */
codex_projector_t *codex_projector_new(void);
void codex_projector_free(codex_projector_t *p);

/* Reset projector state (new turn) */
void codex_projector_reset(codex_projector_t *p);

/* Project a single notification. Returns projection result.
 * Caller must free with codex_projection_free(). */
codex_projection_t *codex_projector_project(codex_projector_t *p, const char *notification_json);

/* Free a projection result */
void codex_projection_free(codex_projection_t *proj);

#ifdef __cplusplus
}
#endif

#endif /* CODEX_EVENT_PROJECTOR_H */
