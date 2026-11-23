#ifndef NOC_W7_SETUPTTS_E5DEE4_H
#define NOC_W7_SETUPTTS_E5DEE4_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_setuptts_e5dee4_provider_label @ hermes_cli/setup_tts.py:_TTS_PROVIDER_LABELS
 * Short label = menu label split at its first " (". NULL result is JSON null. */
struct json_t *w7_setuptts_e5dee4_provider_label(const char *key);

/* PoP: w7_setuptts_e5dee4_all_labels @ hermes_cli/setup_tts.py:_TTS_PROVIDER_LABELS */
struct json_t *w7_setuptts_e5dee4_all_labels(void);

/* PoP: w7_setuptts_e5dee4_tts_api_key_step @ hermes_cli/setup_tts.py:_tts_api_key_step
 * env is the injected map for _setup.get_env_value; prompt_value is the injected
 * return of _setup.prompt. Returns the selected provider or "edge". */
struct json_t *w7_setuptts_e5dee4_tts_api_key_step(const char *selected,
                                                  const struct json_t *env,
                                                  const struct json_t *prompt_value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_SETUPTTS_E5DEE4_H */
