#ifndef AGENT_TURN_CONTEXT_H
#define AGENT_TURN_CONTEXT_H
#include <stdbool.h>
bool agent_turn_context_compression_made_progress(long orig_len, long new_len, long orig_tokens, long new_tokens);
#endif
