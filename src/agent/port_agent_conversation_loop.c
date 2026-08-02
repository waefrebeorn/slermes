/*
 * port_agent_conversation_loop.c — Port of Python agent/conversation_loop.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _stored_prompt_matches_runtime */
bool stored_prompt_matches_runtime(const char *stored_model, const char *current_model) {
    if (!stored_model || !current_model) return true;
    if (stored_model[0] == '\0' || current_model[0] == '\0') return true;
    return (strcmp(stored_model, current_model) == 0);
}


/* Port of Python: _sync_failover_system_message */
/* PoP: sync_failover_system_message @ agent/conversation_loop.py:_sync_failover_system_message */
void sync_failover_system_message(char *api_messages, size_t msg_sz,
                                   const char *active_system_prompt) {
    if (!api_messages || !active_system_prompt) return;
    /* Replace first message (system prompt) with active one */
    /* In C, the system prompt is managed by the agent context */
    /* This function is a no-op in C since the prompt is always current */
    (void)msg_sz;
}

