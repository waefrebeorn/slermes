#ifndef NOC_W13_GATEWAYRUN_DE650A_H
#define NOC_W13_GATEWAYRUN_DE650A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_gatewayrun_de650a_constants @ hermes_cli/gateway_runtime.py (module constants) */
struct json_t *w13_gatewayrun_de650a_constants(void);

/* PoP: w13_gatewayrun_de650a_attachable @ hermes_cli/gateway_runtime.py:attachable
 * reason_for_restart_wait is injected (gateway.drain_control seam). */
struct json_t *w13_gatewayrun_de650a_attachable(const char *state, int has_endpoint,
                                                const char *reason_code, const char *reason_for_restart_wait);

/* PoP: w13_gatewayrun_de650a_control_home_for @ hermes_cli/gateway_runtime.py:control_home_for */
struct json_t *w13_gatewayrun_de650a_control_home_for(const char *home, const char *control_home);

/* PoP: w13_gatewayrun_de650a_endpoint @ hermes_cli/gateway_runtime.py:_endpoint
 * payload is the live identify descriptor; home/control_home are strings.
 * _canonical_home is injected as identity and RESTART_WAIT_REASON as "restart_wait".
 * Returns {"state","reason_code","detail","endpoint":{...}|null}. */
struct json_t *w13_gatewayrun_de650a_endpoint(const struct json_t *payload, const char *home,
                                              const char *control_home);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_GATEWAYRUN_DE650A_H */
