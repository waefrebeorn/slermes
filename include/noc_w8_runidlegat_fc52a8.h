#ifndef NOC_W8_RUNIDLEGAT_FC52A8_H
#define NOC_W8_RUNIDLEGAT_FC52A8_H

/*
 * Public C API for the port of gateway/run_idle_gates.py (gate w8_runidlegat_fc52a8).
 * All exported symbols are runidlegat_-prefixed to avoid collisions.
 * The store facts (availability, store_has_work result, probe failure) are
 * injected via a JSON object exactly as the oracle stubs the goal-cached store.
 */
#include "hermes_json.h"

/* PoP: runidlegat_gate @ gateway/run_idle_gates.py:_gate
 * facts: {"db": bool, "has_work": bool, "raises": bool}. Fails open. */
json_t *runidlegat_gate(const json_t *facts);

/* PoP: runidlegat_profile_has_active_heartbeat @ gateway/run_idle_gates.py:profile_has_active_heartbeat */
json_t *runidlegat_profile_has_active_heartbeat(const json_t *facts);

/* PoP: runidlegat_profile_has_active_loop @ gateway/run_idle_gates.py:profile_has_active_loop */
json_t *runidlegat_profile_has_active_loop(const json_t *facts);

/* PoP: runidlegat_profile_has_pending_handoff @ gateway/run_idle_gates.py:profile_has_pending_handoff */
json_t *runidlegat_profile_has_pending_handoff(const json_t *facts);

/* PoP: runidlegat_off_loop_gate @ gateway/run_idle_gates.py:off_loop_gate */
json_t *runidlegat_off_loop_gate(int has_offload, int offload_result);

#endif /* NOC_W8_RUNIDLEGAT_FC52A8_H */
