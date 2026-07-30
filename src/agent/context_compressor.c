/*
 * context_compressor.c — Port of Python agent/context_compressor.py
 *
 * Python API → C implementation mapping:
 *   compress_context()        → llm_compress_context() in llm_client.c (hermes_agent.h:155)
 *   truncate_context()        → llm_truncate_context() in context.c (hermes_agent.h:150)
 *   estimate_tokens()         → llm_estimate_tokens() inline in hermes_agent.h:141
 *   count_context_tokens()    → llm_count_context_tokens() in hermes_agent.h:147
 *   should_compress()         → handled inline in run_conversation (compression_feedback + cooldown)
 *   get_compression_ratio()   → N/A (ratio computed inline in llm_compress_context())
 *
 * All ported functions exist with their C names in llm_client.c / context.c.
 * This file is name-parity only — the actual compression logic is in
 * llm_client.c (LLM-based summarization), context.c (truncation/eviction),
 * and conversation_loop.c (adaptive threshold, cooldown, locking).
 *
 * Key signatures in hermes_agent.h:
 *   char *llm_compress_context(agent_state_t *state, size_t max_tokens, bool enabled);
 *   void  llm_truncate_context(agent_state_t *state, size_t max_tokens);
 *   size_t llm_count_context_tokens(const message_t **msgs, size_t count, size_t max_tokens);
 *   void  context_evict_smart(agent_state_t *state, size_t max_messages,
 *                             eviction_strategy_t strategy);
 */

#include "hermes_agent.h"   /* llm_compress_context(), llm_truncate_context(), llm_estimate_tokens() */
