#ifndef NOC_W7_INTERRUPTSC_96FC03_H
#define NOC_W7_INTERRUPTSC_96FC03_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: interruptsc_default_tool_reason @ agent/interrupt_scope.py:_TOOL_REASON_HOST_CANCELLED */
const char *interruptsc_default_tool_reason(void);

/* PoP: interruptsc_replay @ agent/interrupt_scope.py:InterruptScope.cancel
 *      interruptsc_replay @ agent/interrupt_scope.py:InterruptScope.track
 *      interruptsc_replay @ agent/interrupt_scope.py:track_in_interrupt_scope
 * Replays a JSON array of ops ({"op":..., ...}) against the latch/registry
 * state machine and returns {reason, tool_reason, interrupts}. */
struct json_t *interruptsc_replay(const struct json_t *ops);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_INTERRUPTSC_96FC03_H */
