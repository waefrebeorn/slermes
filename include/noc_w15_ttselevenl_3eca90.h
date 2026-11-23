#ifndef NOC_W15_TTSELEVENL_3ECA90_H
#define NOC_W15_TTSELEVENL_3ECA90_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_ttselevenl_3eca90_clip @ tools/tts_elevenlabs_voices.py:_clip */
struct json_t *w15_ttselevenl_3eca90_clip(const struct json_t *text);
/* PoP: w15_ttselevenl_3eca90_voice_row @ tools/tts_elevenlabs_voices.py:_voice_row */
struct json_t *w15_ttselevenl_3eca90_voice_row(const struct json_t *voice);
/* PoP: w15_ttselevenl_3eca90_request_params @ tools/tts_elevenlabs_voices.py:_request_params */
struct json_t *w15_ttselevenl_3eca90_request_params(const char *source, const char *query,
                                                   const struct json_t *filters, const char *use_case,
                                                   const char *sort, const char *page, long long page_size);
/* PoP: w15_ttselevenl_3eca90_next_page @ tools/tts_elevenlabs_voices.py:_next_page */
struct json_t *w15_ttselevenl_3eca90_next_page(const char *source, const struct json_t *body, const char *page);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_TTSELEVENL_3ECA90_H */
