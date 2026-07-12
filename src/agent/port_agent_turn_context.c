/* Slermes C port — agent/turn_context.py (pure helper) */

#include <stdbool.h>
#include <stddef.h>

/* PoP: agent_turn_context__compression_made_progress @ agent/turn_context.py:_compression_made_progress */
bool agent_turn_context_compression_made_progress(
    long orig_len, long new_len, long orig_tokens, long new_tokens)
{
    if (new_len < orig_len) return true;
    return orig_tokens > 0 && new_tokens < orig_tokens * 0.95;
}
