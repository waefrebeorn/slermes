#ifndef NOC_W8_RUNBOOTSTRA_8BA685_H
#define NOC_W8_RUNBOOTSTRA_8BA685_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_runbootstra_8ba685_identify_payload @ gateway/run_bootstrap.py:_start_gateway_start_control_socket:_identify_runtime
 * Ports the nested identify payload builder. Args: base (build_identify_payload() result),
 * descriptor (runner.session_runtime_descriptor), draining (bool), restart_wait_reason (string).
 * Returns the identify payload object. Caller frees. */
struct json_t *w8_runbootstra_8ba685_identify_payload(const struct json_t *base,
    const struct json_t *descriptor, const struct json_t *draining,
    const char *restart_wait_reason);

/* PoP: w8_runbootstra_8ba685_browse_reconcile_reserve_wait_s @ gateway/run_bootstrap.py:_browse_reconcile_reserve_wait_s
 * Explicit seam: budget = persistence.core.admin.BROWSE_RECONCILE_BUDGET_S.
 * Returns budget + 2.0 as a JSON number. Caller frees. */
struct json_t *w8_runbootstra_8ba685_browse_reconcile_reserve_wait_s(const struct json_t *budget);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_RUNBOOTSTRA_8BA685_H */
