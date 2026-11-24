/*
 * noc_w20_chatcomple_449db3.h -- public C API for the port of
 *   agent/chat_completion_helpers_relay.py (wave 20 batch 9).
 *
 * RelayChatAccumulator rebuilds a chat.completion from post-intercept chunk
 * dicts. The accumulator is a value: create, observe every chunk, finalize
 * once. Streaming tool-call deltas are assembled by the same slot rules the
 * live `_ToolCallAccumulator` documents (Ollama index reuse, name assignment,
 * argument-part joining, extra_content passthrough).
 */
#ifndef NOC_W20_CHATCOMPLE_449DB3_H
#define NOC_W20_CHATCOMPLE_449DB3_H

#include "hermes_json.h"

typedef struct w20_chatcomple_449db3_acc w20_chatcomple_449db3_acc;

w20_chatcomple_449db3_acc *w20_chatcomple_449db3_new(void);
void w20_chatcomple_449db3_free(w20_chatcomple_449db3_acc *acc);

/* RelayChatAccumulator.observe(chunk) */
void w20_chatcomple_449db3_observe(w20_chatcomple_449db3_acc *acc, const json_t *chunk);

/* RelayChatAccumulator.finalize() */
json_t *w20_chatcomple_449db3_finalize(const w20_chatcomple_449db3_acc *acc);

/* Convenience: observe every element of `chunks`, then finalize. */
json_t *w20_chatcomple_449db3_run(const json_t *chunks);

/* session_rules.turns.flatten_message_text(content, sep="") */
char *w20_chatcomple_449db3_flatten_text(const json_t *content);

/* agent.reasoning_summaries.separate_glued_reasoning_blocks(previous, delta) */
char *w20_chatcomple_449db3_separate_reasoning(const char *previous, const json_t *delta);

#endif /* NOC_W20_CHATCOMPLE_449DB3_H */
