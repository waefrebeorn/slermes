#ifndef NOC_W18_SESSIONMUT_44A137_H
#define NOC_W18_SESSIONMUT_44A137_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_sessionmut_44a137_selected_provider_key_env @ gateway/session_mutation_model.py:_selected_provider_key_env
 * PoP: w18_sessionmut_44a137_is_model_reset @ gateway/session_mutation_model.py:is_model_reset
 *
 * ``_selected_provider_key_env`` returns a string or null (caller frees).
 * ``is_model_reset`` returns a bool; on the reset-with-flags refusal it returns
 * an object {"refused": "invalid_params", "field": "payload", "detail": <text>}.
 */
struct json_t *w18_sessionmut_44a137_selected_provider_key_env(
    const struct json_t *config, const char *provider);
struct json_t *w18_sessionmut_44a137_is_model_reset(const struct json_t *payload);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_SESSIONMUT_44A137_H */
