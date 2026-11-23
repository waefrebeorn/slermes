#ifndef NOC_W11_TOOLPROGRE_3F5E37_H
#define NOC_W11_TOOLPROGRE_3F5E37_H

#include <hermes_json.h>

/* C11 ports of agent/tool_progress_relay.py (wire helpers).  Every symbol is
 * prefixed w11_toolprogre_3f5e37_.  Each takes one borrowed json_t *args
 * object and returns a NEW json_t * (caller frees). */
json_t *w11_toolprogre_3f5e37_payload(json_t *a);
json_t *w11_toolprogre_3f5e37_current(json_t *a);

#endif /* NOC_W11_TOOLPROGRE_3F5E37_H */
