/*
 * tool_executor_deadline.c — C port of upstream Hermes
 * agent/tool_executor_deadline.py.
 * PoP: deadline_declared_floor @ agent/tool_executor_deadline.py:declared_deadline_floor
 * PoP: deadline_with_floor @ agent/tool_executor_deadline.py:deadline_with_floor
 * PoP: deadline_batch @ agent/tool_executor_deadline.py:batch_deadline
 */
#include "noc_deadline.h"

/* _DECLARED_DEADLINE_GRACE_S = 5.0 */
#define DEADLINE_DECLARED_GRACE_S 5.0

deadline_opt_t deadline_declared_floor(deadline_opt_t reg_floor) {
    /* None stays None; otherwise floor + grace. The registry lookup is an
     * injected input: reg_floor is what registry.get_deadline_floor returned. */
    if (!reg_floor.present) return reg_floor;
    reg_floor.value += DEADLINE_DECLARED_GRACE_S;
    return reg_floor;
}

deadline_opt_t deadline_with_floor(deadline_opt_t generic, deadline_opt_t floor) {
    /* A disabled generic deadline (None) stays unbounded; a floor never
     * introduces a deadline, only lengthens one. */
    if (!generic.present || !floor.present) return generic;
    return generic.value >= floor.value ? generic : floor;
}

deadline_opt_t deadline_batch(deadline_opt_t generic,
                              const int *parse_error,
                              const deadline_opt_t *declared, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (parse_error[i]) continue; /* call.parse_error is not None -> skip */
        generic = deadline_with_floor(generic, declared[i]);
    }
    return generic;
}
