#ifndef NOC_W21_APISERVERT_4998C3_H
#define NOC_W21_APISERVERT_4998C3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/platforms/api_server_turn_boundary.py (gate
 * w21_apiservert_4998c3).
 *
 * Where does this turn begin inside the FULL transcript an OpenAI-compat route
 * gets back from `run_conversation`? Two helpers answer it:
 *   _same_transcript_prefix  -- prefix equality by what each message *says*
 *     (role/content/tool_calls/tool_call_id), ignoring the agent's own
 *     timestamp/_db_persisted/reasoning/finish_reason stamps;
 *   response_turn_start_index -- anchored on this turn's user row, with the
 *     prefix match kept as the fallback for legacy/mocked suffixes.
 *
 * The anchor itself, `agent.turn_context.reanchor_current_turn_user_idx`, ranks
 * rows with `session_rules.turns.user_originated_turn_view(msg)`. That
 * projection belongs to the session-rules layer, so it is a seam: the caller
 * passes `live_views`, an array aligned with `result["messages"]` whose i-th
 * entry is that row's live human-authored view (`null` when it has none). The
 * ranking walk over those views is ported here.
 */

/* PoP: w21_apiservert_4998c3_same_prefix @ gateway/platforms/api_server_turn_boundary.py:_same_transcript_prefix
 * True when agent_messages starts with prefix and every zipped pair agrees on
 * the four identity keys (or on the value itself when either is not a dict). */
json_t *w21_apiservert_4998c3_same_prefix(const json_t *agent_messages,
                                          const json_t *prefix);

/* PoP: w21_apiservert_4998c3_start_index @ gateway/platforms/api_server_turn_boundary.py:response_turn_start_index
 * 0-based index where this turn's rows begin (0 = the whole list is the turn). */
json_t *w21_apiservert_4998c3_start_index(const json_t *conversation_history,
                                          const json_t *user_message,
                                          const json_t *result,
                                          const json_t *live_views);

/* PoP: w21_apiservert_4998c3_reanchor @ agent/turn_context.py:reanchor_current_turn_user_idx
 * The anchor walk alone: -1 when no user-originated row exists. Exposed so the
 * exact/live-view/fallback ranking is gated on its own. */
json_t *w21_apiservert_4998c3_reanchor(const json_t *agent_messages,
                                       const json_t *user_message,
                                       const json_t *live_views);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_APISERVERT_4998C3_H */
