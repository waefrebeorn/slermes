#ifndef PORT_AGENT_INTENT_ACK_H
#define PORT_AGENT_INTENT_ACK_H

#include <stdbool.h>

/* C port of intent_ack_continuation_* from agent/agent_runtime_helpers.py.
 * Inputs are the resolved agent attributes (mode/api_mode/model) rather than
 * the agent object, so the helpers are pure & testable. */
char *intent_ack_continuation_mode(const char *mode, const char *api_mode,
                                   const char *model);
bool  intent_ack_continuation_enabled(const char *mode, const char *api_mode,
                                      const char *model);

#endif /* PORT_AGENT_INTENT_ACK_H */
