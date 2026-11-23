#ifndef NOC_W7_ROUTEVERDIC_40E4A2_H
#define NOC_W7_ROUTEVERDIC_40E4A2_H

#include <stdbool.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_routeverdic_40e4a2_route_visit @ agent/route_verdicts.py:route_visit
 * Number of reset_route_verdicts calls; a non-int value (string/float) is 0. */
long long w7_routeverdic_40e4a2_route_visit(const json_t *visit);

/* PoP: w7_routeverdic_40e4a2_snapshot @ agent/route_verdicts.py:snapshot_route_verdicts
 * Present ROUTE_VERDICT_FIELDS of the agent object. Caller frees. */
json_t *w7_routeverdic_40e4a2_snapshot(const json_t *agent);

/* PoP: w7_routeverdic_40e4a2_apply @ agent/route_verdicts.py:apply_route_verdicts
 * Returns the agent object with every value set. Caller frees. */
json_t *w7_routeverdic_40e4a2_apply(const json_t *agent, const json_t *values);

/* PoP: w7_routeverdic_40e4a2_reset @ agent/route_verdicts.py:reset_route_verdicts
 * Returns the agent object after a route change: visit bumped, rejection
 * verdicts false, streaming seed restored, codex replay re-armed. Caller frees. */
json_t *w7_routeverdic_40e4a2_reset(const json_t *agent, bool streaming_seed);

/* PoP: w7_routeverdic_40e4a2_restore_primary @ agent/route_verdicts.py:restore_primary_route_verdicts
 * `endpoint` is the already-normalized agent base_url seam. Returns
 * [rt_after, agent_after]. Caller frees. */
json_t *w7_routeverdic_40e4a2_restore_primary(const json_t *rt, const char *endpoint,
                                              const json_t *agent);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_ROUTEVERDIC_40E4A2_H */
