#ifndef NOC_W6_IDLESCHEDUL_B709D3_H
#define NOC_W6_IDLESCHEDUL_B709D3_H

#include "hermes_json.h"

/* PoP: w6_idleschedul_b709d3_parse_iso_ok @ agent/idle_schedule.py:parse_iso
 * Returns 1 when datetime.fromisoformat(ts) succeeds (ts truthy), else 0. */
int w6_idleschedul_b709d3_parse_iso_ok(const char *ts);

/* PoP: w6_idleschedul_b709d3_interval_due @ agent/idle_schedule.py:interval_due
 * Runs the interval gate on a COPY of `state` and returns
 *   {"due": <bool>, "state": <mutated state>}
 * mirroring the in-place state mutation + boolean return of the Python fn.
 * `now_iso` is an ISO-8601 string parsed as the `now` datetime; seed_summary
 * is stored as last_run_summary when the first observation seeds last_run_at. */
json_t *w6_idleschedul_b709d3_interval_due(const json_t *state, double interval_hours,
                                           const char *now_iso, const char *seed_summary);

#endif /* NOC_W6_IDLESCHEDUL_B709D3_H */
