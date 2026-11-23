#ifndef NOC_W11_TURNEMPTY_94BDEF_H
#define NOC_W11_TURNEMPTY_94BDEF_H

#include <hermes_json.h>

/* C11 ports of agent/turn_empty_response.py (pure helpers).  Every symbol is
 * prefixed w11_turnempty_94bdef_.  Each takes one borrowed json_t *args
 * object and returns a NEW json_t * (caller frees). */
json_t *w11_turnempty_94bdef_latest_display(json_t *a);
json_t *w11_turnempty_94bdef_machinery_quiet(json_t *a);
json_t *w11_turnempty_94bdef_retry_hint(json_t *a);

#endif /* NOC_W11_TURNEMPTY_94BDEF_H */
