#ifndef NOC_W11_SESSIONPER_908F54_H
#define NOC_W11_SESSIONPER_908F54_H

#include <hermes_json.h>

/* C11 ports of agent/session_persistence.py (pure helpers).  Every symbol
 * is prefixed w11_sessionper_908f54_.  Each takes one borrowed json_t *args
 * object and returns a NEW json_t * (caller frees). */
json_t *w11_sessionper_908f54_override(json_t *a);
json_t *w11_sessionper_908f54_content_override(json_t *a);
json_t *w11_sessionper_908f54_safe_filename(json_t *a);

#endif /* NOC_W11_SESSIONPER_908F54_H */
