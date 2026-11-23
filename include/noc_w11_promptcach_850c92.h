#ifndef NOC_W11_PROMPTCACH_850C92_H
#define NOC_W11_PROMPTCACH_850C92_H

#include <hermes_json.h>

/* C11 ports of the pure entry/session helpers in agent/prompt_cache_routes.py.
 * Every symbol is prefixed w11_promptcach_850c92_.  Each function takes one
 * borrowed json_t *args object and returns a NEW json_t * the caller frees. */
json_t *w11_promptcach_850c92_finite(json_t *a);
json_t *w11_promptcach_850c92_numbers(json_t *a);
json_t *w11_promptcach_850c92_well_formed(json_t *a);
json_t *w11_promptcach_850c92_push(json_t *a);
json_t *w11_promptcach_850c92_sessions(json_t *a);
json_t *w11_promptcach_850c92_routine_hit(json_t *a);
json_t *w11_promptcach_850c92_learned_lifetime(json_t *a);
json_t *w11_promptcach_850c92_display_rule(json_t *a);

#endif /* NOC_W11_PROMPTCACH_850C92_H */
