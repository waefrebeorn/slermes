/*
 * port_gateway_run_agent.h — Faithful C11 port of the GatewayRunner turn core
 * (gateway/run.py:GatewayRunner._run_agent_inner).
 *
 * This is the message→agent execution foundation: it wires the session-state
 * helpers (model overrides, reasoning resolution, run-generation guard,
 * sidecar notes, native images) around the agent's run_conversation() call and
 * assembles the faithful result dict the delivery path consumes.
 *
 * Opaque: consumers see only the API; the runner + agent structs stay hidden.
 */
#ifndef PORT_GATEWAY_RUN_AGENT_H
#define PORT_GATEWAY_RUN_AGENT_H

#include "hermes_gateway_runner.h"
#include "hermes_core_types.h"
#include "hermes_json.h"

/* Inputs for one agent turn (mirrors _run_agent_inner parameters that survive
 * the pure-C port: the async/adapter/lease machinery is handled by the caller
 * in server.c's process_update). */
typedef struct {
    const char *message;          /* user turn text */
    const char *context_prompt;   /* system/context prompt (may be NULL) */
    const char *session_key;      /* platform:chat_id override key */
    const char *session_id;       /* persisted session id */
    const char *platform;         /* source platform (for sanitize) */
    const char *observed_context; /* observed group context (may be NULL) */
    int         run_generation;   /* generation token from begin_session_run_generation */
} gw_turn_input_t;

/*
 * gateway_runner_run_agent_inner — execute one agent turn faithfully.
 *
 * Pipeline (mirrors _run_agent_inner):
 *   1. Apply /model session override to the agent (model + runtime kwargs).
 *   2. Resolve session reasoning config (session > per-model > global).
 *   3. Consume pending native image paths → wrap message as multimodal.
 *   4. Wrap with observed group context.
 *   5. Guard: if the run generation is stale, abort (return NULL).
 *   6. Call run_conversation(agent, message, context_prompt).
 *   7. Assemble the result dict: final_response, messages, api_calls,
 *      tokens, model, interrupted, session_id, ...
 *   8. Normalize empty responses, auto-append MEDIA tags, sanitize.
 *   9. Consume one-shot sidecar notes (attached to result meta).
 *
 * Returns a malloc'd json_t result-dict object (caller owns), or NULL when the
 * run generation was invalidated mid-flight (superseded turn — drop silently).
 */
json_t *gateway_runner_run_agent_inner(GatewayRunner *self,
                                       agent_state_t *agent,
                                       const gw_turn_input_t *in);

/* Extract just the user-facing text from a result dict (final_response),
 * or "" when absent. Borrowed pointer into the result dict — do not free. */
const char *gw_turn_result_final_response(const json_t *result);

#endif /* PORT_GATEWAY_RUN_AGENT_H */
