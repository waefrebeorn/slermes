#ifndef NOC_W13_TRANSCRIPTI_F10AE7_H
#define NOC_W13_TRANSCRIPTI_F10AE7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_transcripti_f10ae7_normalize_local_stt_language @ tools/transcription_local.py:_normalize_local_stt_language */
struct json_t *w13_transcripti_f10ae7_normalize_local_stt_language(const struct json_t *language,
                                                                   const struct json_t *supported_languages);

/* PoP: w13_transcripti_f10ae7_normalize_local_model @ tools/transcription_local.py:_normalize_local_model */
struct json_t *w13_transcripti_f10ae7_normalize_local_model(const struct json_t *model_name);

/* PoP: w13_transcripti_f10ae7_get_idle_unload_seconds @ tools/transcription_local.py:_get_idle_unload_seconds */
struct json_t *w13_transcripti_f10ae7_get_idle_unload_seconds(const struct json_t *local_cfg);

/* PoP: w13_transcripti_f10ae7_looks_like_cuda_lib_error @ tools/transcription_local.py:_looks_like_cuda_lib_error */
struct json_t *w13_transcripti_f10ae7_looks_like_cuda_lib_error(const struct json_t *exc_msg);

/* PoP: w13_transcripti_f10ae7_confidence_thresholds @ tools/transcription_local.py:_confidence_thresholds
 * Returns [no_speech_prob, avg_logprob]. */
struct json_t *w13_transcripti_f10ae7_confidence_thresholds(const struct json_t *local_cfg);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_TRANSCRIPTI_F10AE7_H */
