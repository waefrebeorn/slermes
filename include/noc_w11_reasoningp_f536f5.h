#ifndef NOC_W11_REASONINGP_F536F5_H
#define NOC_W11_REASONINGP_F536F5_H

#include <hermes_json.h>

/* C11 port of agent/reasoning_promotion.py:answer_in_reasoning_capability.
 * The four external seams (host match, config capability, route entries,
 * local-endpoint probe) are injected through the args object, matching the
 * oracle stubs.  Takes a borrowed json_t *args, returns a NEW json_t *bool. */
json_t *w11_reasoningp_f536f5_capability(json_t *a);

#endif /* NOC_W11_REASONINGP_F536F5_H */
