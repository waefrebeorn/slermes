#ifndef NOC_W8_VOICETURNR_3C1C02_H
#define NOC_W8_VOICETURNR_3C1C02_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: voiceturnr_route_target @ agent/voice_turn_route.py:_route_target
 *
 * Heavy collaborators (_get_aux_model_for_provider, expand_direct_api_alias)
 * are injected through the fixture exactly as the oracle registers them. */
struct json_t *voiceturnr_route_target(struct json_t *cfg,
    const char *agent_provider, const char *agent_model,
    const char *aux_model, const char *alias_provider, const char *alias_base);

/* PoP: voiceturnr_voice_reasoning @ agent/voice_turn_route.py:_voice_reasoning
 *
 * _lowest_effort() is injected as the fixture's `lowest` seam. */
struct json_t *voiceturnr_voice_reasoning(struct json_t *effort, const char *lowest);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_VOICETURNR_3C1C02_H */
