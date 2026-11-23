#ifndef NOC_W10_REASONINGC_80BDDD_H
#define NOC_W10_REASONINGC_80BDDD_H

#include <hermes_json.h>

/* C11 ports of agent/reasoning_carriers.py.  Every symbol is prefixed
 * w10_reasoningc_80bddd_.  Each takes one borrowed json_t *args object and
 * returns a NEW json_t *.  Provider-profile key declarations and replay hook
 * answers are injected through the args object. */
json_t *w10_reasoningc_80bddd_thought_signature(json_t *a);
json_t *w10_reasoningc_80bddd_carrier_record(json_t *a);
json_t *w10_reasoningc_80bddd_strip_function_signature(json_t *a);
json_t *w10_reasoningc_80bddd_replay_plan(json_t *a);
json_t *w10_reasoningc_80bddd_shape(json_t *a);

#endif /* NOC_W10_REASONINGC_80BDDD_H */
