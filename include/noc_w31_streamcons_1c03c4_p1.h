/*
 * noc_w31_streamcons_1c03c4_p1.h - C surface for upstream
 * gateway/stream_consumer_transport.py, gate w31_streamcons_1c03c4_p1.
 *
 * Framing / preview-state half of the transport: which preview ids a fresh
 * final replaces, whether a real (editable) preview exists, where an adopted
 * message id comes from, and the two state resets (native close, fallback
 * mode).  Every adapter call is a seam; each function takes one JSON request
 * object and returns the value the upstream method would leave behind.
 */
#ifndef NOC_W31_STREAMCONS_1C03C4_P1_H
#define NOC_W31_STREAMCONS_1C03C4_P1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w31_streamcons_1c03c4_p1_constants @ gateway/stream_consumer_transport.py:<module constants + StreamTransportMixin._MIN_NEW_MSG_CHARS>
 * {"max_edit_backoff_secs": 30.0, "min_new_msg_chars": 4}. */
json_t *w31_streamcons_1c03c4_p1_constants(void);

/* PoP: w31_streamcons_1c03c4_p1_draft_metadata @ gateway/stream_consumer_transport.py:StreamTransportMixin._draft_metadata
 * in {metadata, reply_to} -> the draft metadata dict, or null when empty.
 * reply_to_message_id is set only when absent (setdefault) and reply_to truthy. */
json_t *w31_streamcons_1c03c4_p1_draft_metadata(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p1_stale_preview_ids @ gateway/stream_consumer_transport.py:StreamTransportMixin._stale_preview_ids
 * in {preview_ids, segment_preview_ids, message_id, segment_only} ->
 * sorted unique id list; segment_only reads the segment set and str()s the
 * message id, and "__no_edit__" is never stale. */
json_t *w31_streamcons_1c03c4_p1_stale_preview_ids(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p1_has_real_preview @ gateway/stream_consumer_transport.py:StreamTransportMixin._has_real_preview
 * in {message_id} -> `bool(message_id) and message_id != "__no_edit__"`. */
int w31_streamcons_1c03c4_p1_has_real_preview(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p1_adopt_message_id @ gateway/stream_consumer_transport.py:StreamTransportMixin._adopt_message_id
 * in {message_id} -> {"message_id": str|null, "created": bool}: a truthy id is
 * adopted (clock stamped), anything else becomes "__no_edit__" with no clock. */
json_t *w31_streamcons_1c03c4_p1_adopt_message_id(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p1_close_native_state @ gateway/stream_consumer_transport.py:StreamTransportMixin._close_native_state
 * in {already_sent} -> {"native_stream_opened": false,
 * "native_last_pushed_len": 0, "already_sent": <unchanged>}. */
json_t *w31_streamcons_1c03c4_p1_close_native_state(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p1_enter_fallback_mode @ gateway/stream_consumer_transport.py:StreamTransportMixin._enter_fallback_mode
 * in {prefix} -> {fallback_prefix, fallback_final_send: true,
 * edit_supported: false, already_sent: true}. */
json_t *w31_streamcons_1c03c4_p1_enter_fallback_mode(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p1_track_preview_id @ gateway/stream_consumer_transport.py:StreamTransportMixin._track_preview_id
 * in {message_id, preview_ids, segment_preview_ids} -> both sets after the
 * track (a real id joins each), each as a sorted unique list. */
json_t *w31_streamcons_1c03c4_p1_track_preview_id(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W31_STREAMCONS_1C03C4_P1_H */
