#ifndef NOC_W11_PLUGINSESS_A37456_H
#define NOC_W11_PLUGINSESS_A37456_H

#include <hermes_json.h>

/* C11 ports of the pure helpers in agent/plugin_session_fork.py.
 * Every exported symbol is prefixed w11_pluginsess_a37456_.
 * Each function takes one borrowed json_t *args object (the whole fixture
 * case) and returns a NEW json_t * the caller frees. */
json_t *w11_pluginsess_a37456_bounded_int(json_t *a);
json_t *w11_pluginsess_a37456_clamped_timeout(json_t *a);
json_t *w11_pluginsess_a37456_constants(json_t *a);

#endif /* NOC_W11_PLUGINSESS_A37456_H */
