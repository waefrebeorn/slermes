#ifndef NOC_W15_TRANSCRIPTI_57B86B_H
#define NOC_W15_TRANSCRIPTI_57B86B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_transcripti_57b86b_enforce_prompt @ tools/transcription_command.py:_enforce_prompt_length_limit */
struct json_t *w15_transcripti_57b86b_enforce_prompt(const struct json_t *prompt, const char *provider);
/* PoP: w15_transcripti_57b86b_unregistered_error @ tools/transcription_command.py:_unregistered_stt_provider_error */
struct json_t *w15_transcripti_57b86b_unregistered_error(const struct json_t *provider);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_TRANSCRIPTI_57B86B_H */
