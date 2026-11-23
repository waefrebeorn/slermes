#ifndef NOC_W14_DELEGATETO_56C3A1_H
#define NOC_W14_DELEGATETO_56C3A1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_delegateto_56c3a1_param_error @ tools/delegate_tool_routes.py:_param_error */
struct json_t *w14_delegateto_56c3a1_param_error(long long index, const char *name, const struct json_t *value,
                               const struct json_t *schema);
/* PoP: w14_delegateto_56c3a1_clean_route @ tools/delegate_tool_routes.py:_clean_route */
struct json_t *w14_delegateto_56c3a1_clean_route(const char *provider_name, const struct json_t *route);
/* PoP: w14_delegateto_56c3a1_with_inherited_fallback @ tools/delegate_tool_routes.py:_with_inherited_fallback */
struct json_t *w14_delegateto_56c3a1_with_inherited_fallback(const struct json_t *route, const struct json_t *config);
/* PoP: w14_delegateto_56c3a1_route_report @ tools/delegate_tool_routes.py:TaskRoute.report */
struct json_t *w14_delegateto_56c3a1_route_report(const char *backend, const struct json_t *label);
/* PoP: w14_delegateto_56c3a1_route_refused_entry @ tools/delegate_tool_routes.py:TaskRoute.refused_entry */
struct json_t *w14_delegateto_56c3a1_route_refused_entry(long long index, const char *backend, const struct json_t *label,
                                       const struct json_t *error);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_DELEGATETO_56C3A1_H */
