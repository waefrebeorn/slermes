#ifndef NOC_W9_GATEWAYSIDE_3248C8_H
#define NOC_W9_GATEWAYSIDE_3248C8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_gatewayside_3248c8_is_side_event @ hermes_cli/gateway_side.py:SIDE_EVENTS */
int w9_gatewayside_3248c8_is_side_event(const char *name);

/* PoP: w9_gatewayside_3248c8_connection_for @ hermes_cli/gateway_side.py:connection_for
 * The store/session/final state is injected as ints instead of live objects. */
int w9_gatewayside_3248c8_connection_for(int has_store, int session_open, int final);

/* PoP: w9_gatewayside_3248c8_start @ hermes_cli/gateway_side.py:start
 * result is the injected call_tagged() result; returns {"task_id":..} or {"error":..}. */
struct json_t *w9_gatewayside_3248c8_start(const struct json_t *result, const char *verb);

/* PoP: w9_gatewayside_3248c8_answer @ hermes_cli/gateway_side.py:start.<locals>.wait
 * payload is the injected inbox message; returns {"answer":..} or {"error":..}. */
struct json_t *w9_gatewayside_3248c8_answer(const struct json_t *payload);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_GATEWAYSIDE_3248C8_H */
