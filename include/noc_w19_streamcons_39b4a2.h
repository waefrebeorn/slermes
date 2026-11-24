/* noc_w19_streamcons_39b4a2.h - public C API for gateway/stream_consumer_fallback.py. */
#ifndef NOC_W19_STREAMCONS_39B4A2_H
#define NOC_W19_STREAMCONS_39B4A2_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_streamcons_39b4a2_edit_skipped @ gateway/stream_consumer_fallback.py:_edit_skipped */
json_t *w19_streamcons_39b4a2_edit_skipped(json_t *result);

/* PoP: w19_streamcons_39b4a2_visible_prefix @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._visible_prefix
 * The text already shown, cursor decoration removed. _clean_for_display is the caller's. */
json_t *w19_streamcons_39b4a2_visible_prefix(const char *last_sent_text, const char *cursor);

/* PoP: w19_streamcons_39b4a2_continuation_text @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._continuation_text
 * {"text": <the unseen tail>, "fragment_cut": <int|null>} - the cut is set only when the
 * whole answer replaces a deletable first-word fragment. */
json_t *w19_streamcons_39b4a2_continuation_text(const char *final_text, const char *fallback_prefix,
                                               const char *last_sent_text, const char *cursor,
                                               int fragment_can_be_replaced);

/* PoP: w19_streamcons_39b4a2_prose_resume_point @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._prose_resume_point */
json_t *w19_streamcons_39b4a2_prose_resume_point(const char *final_text, long cut);

/* PoP: w19_streamcons_39b4a2_fragment_can_be_replaced @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._fragment_can_be_replaced */
json_t *w19_streamcons_39b4a2_fragment_can_be_replaced(const char *message_id, int preserve_partial,
                                                      int adapter_can_delete);

/* PoP: w19_streamcons_39b4a2_send_failure_may_have_delivered @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._send_failure_may_have_delivered */
json_t *w19_streamcons_39b4a2_send_failure_may_have_delivered(int retryable, int delivery_uncertain,
                                                             const char *error, const char *exc_name);

/* PoP: w19_streamcons_39b4a2_is_flood_error @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._is_flood_error */
json_t *w19_streamcons_39b4a2_is_flood_error(const char *error);

/* PoP: w19_streamcons_39b4a2_flood_retry_delay @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._fallback_flood_retry_delay */
json_t *w19_streamcons_39b4a2_flood_retry_delay(const char *error, int has_retry_after,
                                                double retry_after, double max_wait);

/* PoP: w19_streamcons_39b4a2_adopt_partial_head @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._adopt_partial_head
 * {"adopted":bool, "message_id":.., "last_sent_text":.., "preserve_partial":.., "prefix":..} */
json_t *w19_streamcons_39b4a2_adopt_partial_head(json_t *result, const char *text);

/* PoP: w19_streamcons_39b4a2_raw_message_limit @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._raw_message_limit */
json_t *w19_streamcons_39b4a2_raw_message_limit(int is_base, int has_max, double max_value,
                                                int chat_limit_raises, int has_chat_limit,
                                                double chat_limit, int cap_kind, double cap_value);

/* PoP: w19_streamcons_39b4a2_open_code_fence_span @ gateway/stream_consumer_fences.py:open_code_fence_span
 * {"span":[start,end]} or null when no fence is open. */
json_t *w19_streamcons_39b4a2_open_code_fence_span(const char *text, long upto);
/* PoP: w19_streamcons_39b4a2_ensure_closed_code_fences @ gateway/stream_consumer_fences.py:ensure_closed_code_fences */
json_t *w19_streamcons_39b4a2_ensure_closed_code_fences(const char *text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_STREAMCONS_39B4A2_H */
