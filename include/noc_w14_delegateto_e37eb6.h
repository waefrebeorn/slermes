#ifndef NOC_W14_DELEGATETO_E37EB6_H
#define NOC_W14_DELEGATETO_E37EB6_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_delegateto_e37eb6_without_service_tier @ tools/delegate_tool_tuning.py:without_service_tier
 * Deep copy of overrides without a speed-tier key at the top level or inside
 * extra_body; an emptied extra_body is removed. */
struct json_t *w14_delegateto_e37eb6_without_service_tier(const struct json_t *overrides);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_DELEGATETO_E37EB6_H */
