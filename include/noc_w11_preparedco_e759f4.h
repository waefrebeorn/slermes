#ifndef NOC_W11_PREPAREDCO_E759F4_H
#define NOC_W11_PREPAREDCO_E759F4_H

#include <hermes_json.h>

/* C11 ports of the pure decision helpers in agent/prepared_compaction.py.
 * Agent/compressor state, the oneshot probe result and monotonic "now" are
 * injected through the args object, matching the oracle's stubs.
 * Functions take a borrowed json_t *args object and return a NEW json_t *bool. */
json_t *w11_preparedco_e759f4_blocked_for_platform(json_t *a);
json_t *w11_preparedco_e759f4_blocked(json_t *a);

#endif /* NOC_W11_PREPAREDCO_E759F4_H */
