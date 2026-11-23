#ifndef NOC_W13_TRANSCRIPTI_F24CCF_H
#define NOC_W13_TRANSCRIPTI_F24CCF_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_transcripti_f24ccf_sdk_prompt_kwargs @ tools/transcription_cloud.py:_sdk_prompt_kwargs
 * language/prompt create-kwargs, each only when truthy (fixtures use JSON values). */
struct json_t *w13_transcripti_f24ccf_sdk_prompt_kwargs(const struct json_t *language,
                                                        const struct json_t *prompt);

/* PoP: w13_transcripti_f24ccf_elevenlabs_error_detail @ tools/transcription_cloud.py:_elevenlabs_error_detail */
struct json_t *w13_transcripti_f24ccf_elevenlabs_error_detail(const struct json_t *err_body);

/* PoP: w13_transcripti_f24ccf_extract_transcript_text @ tools/transcription_cloud.py:_extract_transcript_text
 * Returns the normalized transcript string, or {"error": <msg>} when the live
 * function raises STTResponseError (the oracle catches it into the same envelope). */
struct json_t *w13_transcripti_f24ccf_extract_transcript_text(const struct json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_TRANSCRIPTI_F24CCF_H */
