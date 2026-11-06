#ifndef AGENT_INTENT_ACK_H
#define AGENT_INTENT_ACK_H
#include <stdbool.h>  /* bool in the exported signature */
#include <stddef.h>   /* size_t in the exported signature */
/* PoP: agent_intent_ack_continuation_mode @ agent/agent_runtime_helpers.py:intent_ack_continuation_mode */
void agent_intent_ack_continuation_mode(const char *mode, const char *api_mode,
                                        const char *model, char *out, size_t outsz);
bool agent_intent_ack_continuation_enabled(const char *mode, const char *api_mode,
                                           const char *model);
#endif
