#ifndef NOC_W9_INTERRUPTCO_7C8A77_H
#define NOC_W9_INTERRUPTCO_7C8A77_H

#include <hermes_json.h>

/* C11 ports of the pure reason/issuer helpers and steer-admission logic in
 * agent/interrupt_control.py. Every symbol is prefixed w9_interruptco_7c8a77_
 * and takes one borrowed json_t *args object, returning a NEW json_t *. */
json_t *w9_interruptco_7c8a77_interrupt_issuer(json_t *a);
json_t *w9_interruptco_7c8a77_interrupt_skip_wording(json_t *a);
json_t *w9_interruptco_7c8a77_interrupted_during_api_call_reason(json_t *a);
json_t *w9_interruptco_7c8a77_steer(json_t *a);

#endif /* NOC_W9_INTERRUPTCO_7C8A77_H */
