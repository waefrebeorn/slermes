#ifndef NOC_SESSINFO_H
#define NOC_SESSINFO_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: sessinfo_route @ gateway/session_info_route.py:channel_override_route
 *
 * Faithful port of channel_override_route(config, source). Runtime collaborators
 * are injected as data so the pure decision is testable:
 *   source          - the source carrier (JSON null => (None, None, None)), or a dict
 *                     with platform/chat_id/thread_id/parent_chat_id.
 *   config_present  - whether `config` is non-None (Python: config is None -> triple None).
 *   override        - _get_channel_override(...) result: null or {"model":..,"provider":..}.
 *   runtime         - _resolve_runtime_agent_kwargs_for_provider(...) result dict or null.
 *   runtime_raises  - inject the resolver's exception (-> (model, None, provider)).
 *   identity        - session_provider_identity(...) result; falsy (null/"") -> ch.provider.
 *
 * Returns a NEWLY allocated json_t ARRAY [model, route, force_provider] where each
 * element may be JSON null. The caller owns and must json_free() the result.
 */
struct json_t *sessinfo_route(const struct json_t *source, int config_present,
                              const struct json_t *override, const struct json_t *runtime,
                              int runtime_raises, const struct json_t *identity);

#ifdef __cplusplus
}
#endif

#endif /* NOC_SESSINFO_H */
