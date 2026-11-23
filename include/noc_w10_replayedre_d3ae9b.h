#ifndef NOC_W10_REPLAYEDRE_D3AE9B_H
#define NOC_W10_REPLAYEDRE_D3AE9B_H

#include <hermes_json.h>

/* C11 ports of agent/replayed_reasoning.py.  Every symbol is prefixed
 * w10_replayedre_d3ae9b_.  Each function takes one borrowed json_t *args
 * object and returns a NEW json_t *.  The stamped token count (normally
 * message_metadata.stamped_reasoning_tokens) is injected as "stamp". */
json_t *w10_replayedre_d3ae9b_strip_opaque_replay_items(json_t *a);
json_t *w10_replayedre_d3ae9b_legacy_tokens(json_t *a);
json_t *w10_replayedre_d3ae9b_codex_reasoning_replay_tokens(json_t *a);
json_t *w10_replayedre_d3ae9b_signed_thinking_replay_tokens(json_t *a);

#endif /* NOC_W10_REPLAYEDRE_D3AE9B_H */
