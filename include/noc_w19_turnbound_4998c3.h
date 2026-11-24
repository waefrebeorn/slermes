/* noc_w19_turnbound_4998c3.h -- current-turn boundary inside an agent transcript.
 * PoP: w19_turnbound_4998c3_same_prefix @ gateway/platforms/api_server_turn_boundary.py:_same_transcript_prefix
 * PoP: w19_turnbound_4998c3_start_index @ gateway/platforms/api_server_turn_boundary.py:response_turn_start_index
 */
#ifndef NOC_W19_TURNBOUND_4998C3_H
#define NOC_W19_TURNBOUND_4998C3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _same_transcript_prefix(agent_messages, prefix) -> bool */
json_t *w19_turnbound_4998c3_same_prefix(json_t *agent_messages, json_t *prefix);

/* response_turn_start_index(conversation_history, user_message, result)
 * reanchor_idx models the live `reanchor_current_turn_user_idx` seam:
 * reanchor_idx >= 0 means the loop found this turn's user row at that index. */
json_t *w19_turnbound_4998c3_start_index(json_t *conversation_history,
                                        json_t *user_message, json_t *result,
                                        int reanchor_idx);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_TURNBOUND_4998C3_H */
