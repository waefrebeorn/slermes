#ifndef NOC_W14_TRANSCRIPTI_1EC31E_H
#define NOC_W14_TRANSCRIPTI_1EC31E_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes tools/transcription_common.py (gate w14_transcripti_1ec31e).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w14_transcripti_1ec31e_error_result @ tools/transcription_common.py:_error_result
 * `extra` is a JSON object whose keys are merged after the three envelope keys. */
struct json_t *w14_transcripti_1ec31e_error_result(const struct json_t *error,
                                                   const struct json_t *extra);

/* PoP: w14_transcripti_1ec31e_ok_result @ tools/transcription_common.py:_ok_result */
struct json_t *w14_transcripti_1ec31e_ok_result(const struct json_t *transcript,
                                               const struct json_t *provider);

/* PoP: w14_transcripti_1ec31e_normalize_xai_stt_model @ tools/transcription_common.py:normalize_xai_stt_model
 * env_value is the per-call STT_XAI_MODEL read injected from the fixture. */
struct json_t *w14_transcripti_1ec31e_normalize_xai_stt_model(const struct json_t *model,
                                                             const struct json_t *env_value);

/* PoP: w14_transcripti_1ec31e_config_number @ tools/transcription_common.py:_config_number
 * `cast` is the string "float" or "int"; `default` is the numeric fallback. */
struct json_t *w14_transcripti_1ec31e_config_number(const struct json_t *cfg, const char *key,
                                                    const struct json_t *default_value,
                                                    const char *cast);

/* PoP: w14_transcripti_1ec31e_process_error_detail @ tools/transcription_common.py:_process_error_detail
 * stderr/stdout/exc_str seams match the CalledProcessError attributes. */
struct json_t *w14_transcripti_1ec31e_process_error_detail(const struct json_t *err_text,
                                                           const struct json_t *out_text,
                                                           const struct json_t *exc_str);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_TRANSCRIPTI_1EC31E_H */
