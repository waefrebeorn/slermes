#ifndef NOC_W8_SESSIONLOCA_B67E17_H
#define NOC_W8_SESSIONLOCA_B67E17_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_sessionloca_b67e17_effort_fields @ gateway/session_local.py:effort_fields
 * Ports effort_fields + its clamp_effort call. External lookups are injected seams:
 *   supported_efforts = agent.reasoning_effort.route_supported_efforts(...)
 *   static_tiers      = agent.fast_mode.STATIC_TIERS
 *   fast_override     = hermes_cli.models.resolve_fast_mode_overrides(...)
 * Returns {reasoning_effort, reasoning_effort_wire, service_tier, fast}. Caller frees. */
struct json_t *w8_sessionloca_b67e17_effort_fields(const struct json_t *config,
    const struct json_t *tier, const struct json_t *model, const struct json_t *provider,
    const struct json_t *base_url, const struct json_t *api_mode,
    const struct json_t *supported_efforts, const struct json_t *static_tiers,
    const struct json_t *fast_override);

/* PoP: w8_sessionloca_b67e17_identity @ gateway/session_local.py:LocalSessionAdapter._identity
 * Returns [platform, chat_id, user_id, profile, chat_type] from a source object. Caller frees. */
struct json_t *w8_sessionloca_b67e17_identity(const struct json_t *source);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_SESSIONLOCA_B67E17_H */
