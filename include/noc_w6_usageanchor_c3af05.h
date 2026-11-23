#ifndef NOC_W6_USAGEANCHOR_C3AF05_H
#define NOC_W6_USAGEANCHOR_C3AF05_H

/*
 * Public C API for the port of agent/usage_anchor.py (gate w6_usageanchor_c3af05).
 * All exported symbols are usageanchor_-prefixed.
 */
#include "hermes_json.h"

/* message_fingerprint(msg) -> 64-char sha256 hex string, or JSON null. */
json_t *usageanchor_fingerprint(const json_t *msg);

/* capture_usage_anchor(prompt_tokens, completion_tokens, messages) -> anchor object or null. */
json_t *usageanchor_capture(const json_t *prompt_tokens, const json_t *completion_tokens,
                            const json_t *messages);

/* _anchor_matches(messages, anchor) -> bool. */
json_t *usageanchor_anchor_matches(const json_t *messages, const json_t *anchor);

/* _serialize(anchor) -> normalized anchor object or null. */
json_t *usageanchor_serialize(const json_t *anchor);

/* anchored_context_tokens(messages, anchor, delta_estimate) -> JSON int or null.
 * delta_estimate stands in for estimate_messages_tokens_rough(delta). */
json_t *usageanchor_context_tokens(const json_t *messages, const json_t *anchor,
                                   double delta_estimate);

#endif /* NOC_W6_USAGEANCHOR_C3AF05_H */
