/*
 * run_idle_gates.c — C port of upstream Hermes gateway/run_idle_gates.py
 * (gate w8_runidlegat_fc52a8).
 *
 * PoP: runidlegat_gate @ gateway/run_idle_gates.py:_gate
 * PoP: runidlegat_profile_has_active_heartbeat @ gateway/run_idle_gates.py:profile_has_active_heartbeat
 * PoP: runidlegat_profile_has_active_loop @ gateway/run_idle_gates.py:profile_has_active_loop
 * PoP: runidlegat_profile_has_pending_handoff @ gateway/run_idle_gates.py:profile_has_pending_handoff
 * PoP: runidlegat_off_loop_gate @ gateway/run_idle_gates.py:off_loop_gate
 */
#include "noc_w8_runidlegat_fc52a8.h"
#include "hermes_json.h"

/* Every gate fails OPEN: unavailable store, failing read or corrupt row => True. */
static int gate_core(const json_t *facts) {
    if (!facts || !json_get_bool(facts, "db", false)) return 1;
    if (json_get_bool(facts, "raises", false)) return 1;
    return json_get_bool(facts, "has_work", false) ? 1 : 0;
}

json_t *runidlegat_gate(const json_t *facts) {
    return json_bool(gate_core(facts) ? true : false);
}
json_t *runidlegat_profile_has_active_heartbeat(const json_t *facts) {
    return json_bool(gate_core(facts) ? true : false);
}
json_t *runidlegat_profile_has_active_loop(const json_t *facts) {
    return json_bool(gate_core(facts) ? true : false);
}
json_t *runidlegat_profile_has_pending_handoff(const json_t *facts) {
    return json_bool(gate_core(facts) ? true : false);
}
json_t *runidlegat_off_loop_gate(int has_offload, int offload_result) {
    return json_bool((!has_offload || offload_result) ? true : false);
}
