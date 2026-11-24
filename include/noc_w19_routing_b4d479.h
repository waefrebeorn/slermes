/* noc_w19_routing_b4d479.h - public C API for persistence/gateway/routing.py port. */
#ifndef NOC_W19_ROUTING_B4D479_H
#define NOC_W19_ROUTING_B4D479_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* A GatewayRuntime projects as {"provider":..,"base_url":..,"api_mode":..},
 * every unset member null. resume_route projects as {"model":..,"route":{...}}. */

/* PoP: w19_routing_b4d479_route_namespace @ persistence/gateway/routing.py:route_namespace */
const char *w19_routing_b4d479_route_namespace(const char *session_key);
/* PoP: w19_routing_b4d479_normalize_topic_profile_name @ persistence/gateway/routing.py:normalize_topic_profile_name */
const char *w19_routing_b4d479_normalize_topic_profile_name(const char *profile_name);
/* PoP: w19_routing_b4d479_route @ persistence/gateway/routing.py:_route */
json_t *w19_routing_b4d479_route(json_t *members, int keep_falsy);
/* PoP: w19_routing_b4d479_gateway_runtime @ persistence/gateway/routing.py:gateway_runtime */
json_t *w19_routing_b4d479_gateway_runtime(json_t *model_config, const char *billing_provider,
                                           const char *billing_base_url);
/* PoP: w19_routing_b4d479_resume_route @ persistence/gateway/routing.py:resume_route
 * model_config may be its JSON text or an object; unparsable text reads as empty. */
json_t *w19_routing_b4d479_resume_route(json_t *model_config, const char *model,
                                        const char *billing_provider, const char *billing_base_url);
/* PoP: w19_routing_b4d479_is_empty @ persistence/gateway/routing.py:GatewayRuntime.is_empty */
int w19_routing_b4d479_is_empty(json_t *runtime);
/* PoP: w19_routing_b4d479_constants @ persistence/gateway/routing.py:<module>
 * The module's vocabulary: end reasons, the adoption gap, the bare billing buckets, the meta key. */
json_t *w19_routing_b4d479_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_ROUTING_B4D479_H */
