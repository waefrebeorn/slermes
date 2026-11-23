#ifndef NOC_W11_STREAMMAX_7580A7_H
#define NOC_W11_STREAMMAX_7580A7_H

#include <hermes_json.h>

/* C11 ports of agent/stream_max_lifetime.py.  Every symbol is prefixed
 * w11_streammax_7580a7_.  Each takes one borrowed json_t *args object and
 * returns a NEW json_t * (caller frees).  Config and NaN/Inf inputs are
 * injected through the args object (string sentinels "__inf__"/"__nan__"). */
json_t *w11_streammax_7580a7_resolve(json_t *a);
json_t *w11_streammax_7580a7_message(json_t *a);

#endif /* NOC_W11_STREAMMAX_7580A7_H */
