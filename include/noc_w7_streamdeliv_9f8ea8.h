#ifndef NOC_W7_STREAMDELIV_9F8EA8_H
#define NOC_W7_STREAMDELIV_9F8EA8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: streamdeliv_normalize_interim_visible_text @ agent/stream_delivery.py:StreamDeliveryMixin._normalize_interim_visible_text */
struct json_t *streamdeliv_normalize_interim_visible_text(const struct json_t *value);
/* PoP: streamdeliv_interim_content_was_streamed @ agent/stream_delivery.py:StreamDeliveryMixin._interim_content_was_streamed */
int streamdeliv_interim_content_was_streamed(const char *content, const char *streamed);
/* PoP: streamdeliv_interim_content_fully_streamed @ agent/stream_delivery.py:StreamDeliveryMixin._interim_content_fully_streamed */
int streamdeliv_interim_content_fully_streamed(const char *content, const char *streamed);
/* PoP: streamdeliv_codex_commentary_texts @ agent/stream_delivery.py:_codex_commentary_texts */
struct json_t *streamdeliv_codex_commentary_texts(const struct json_t *assistant_msg);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_STREAMDELIV_9F8EA8_H */
