/*
 * noc_w31_streamcons_1c03c4_p2.h - C surface for upstream
 * gateway/stream_consumer_transport.py, gate w31_streamcons_1c03c4_p2.
 *
 * Cadence / backpressure / transport-policy half: whether a draft snapshot is
 * due, how a server retry_after moves the cooldown horizon, when a long-lived
 * preview is replaced by a fresh final, what a native degradation leaves
 * behind, which transport a chat resolves to, and what a first send that may
 * have landed is allowed to leave on screen.  The adapter probes, the clock and
 * the delivery ledger are seams: each function takes one JSON request object.
 */
#ifndef NOC_W31_STREAMCONS_1C03C4_P2_H
#define NOC_W31_STREAMCONS_1C03C4_P2_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w31_streamcons_1c03c4_p2_should_push_draft @ gateway/stream_consumer_transport.py:StreamTransportMixin._should_push_draft
 * in {accumulated, now, draft_retry_until, last_edit_time,
 * current_edit_interval} -> bool.  A full buffer never bypasses cadence. */
int w31_streamcons_1c03c4_p2_should_push_draft(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p2_defer_draft_retry @ gateway/stream_consumer_transport.py:StreamTransportMixin._defer_draft_retry
 * in {retry_after, now, draft_retry_until} ->
 * {"deferred": bool, "retry_until": num}.  Only a positive int/float (a bool is
 * an int) defers; the horizon is max(existing, now + retry_after). */
json_t *w31_streamcons_1c03c4_p2_defer_draft_retry(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p2_should_send_fresh_final @ gateway/stream_consumer_transport.py:StreamTransportMixin._should_send_fresh_final
 * in {fresh_final_after_seconds, message_id, message_created_ts, now} -> bool:
 * a positive threshold, a real preview, a known creation time, and enough age. */
int w31_streamcons_1c03c4_p2_should_send_fresh_final(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p2_degrade_native_to_buffered_send @ gateway/stream_consumer_transport.py:StreamTransportMixin._degrade_native_to_buffered_send
 * -> {use_native_streaming: false, native_stream_opened: false,
 * native_last_pushed_len: 0, buffer_only: true}. */
json_t *w31_streamcons_1c03c4_p2_degrade_native_to_buffered_send(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p2_resolve_draft_streaming @ gateway/stream_consumer_transport.py:StreamTransportMixin._resolve_draft_streaming
 * in {transport, adapter_is_base, probe, probe_value} -> bool.  "edit"/"off"
 * (None reads as "edit") and non-BasePlatformAdapter doubles never draft; a
 * raising probe reads as unsupported. */
int w31_streamcons_1c03c4_p2_resolve_draft_streaming(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p2_resolve_native_streaming @ gateway/stream_consumer_transport.py:StreamTransportMixin._resolve_native_streaming
 * in {adapter_is_base, adapter_class_flag, has_probe, probe, probe_value} ->
 * bool: needs a BasePlatformAdapter, the class-level SUPPORTS_NATIVE_STREAMING
 * flag and a truthy probe. */
int w31_streamcons_1c03c4_p2_resolve_native_streaming(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p2_track_preview_ids_from_result @ gateway/stream_consumer_transport.py:StreamTransportMixin._track_preview_ids_from_result
 * in {result: {message_id, continuation_message_ids, raw_response:{message_ids}}}
 * -> {"preview_ids": sorted unique list} (the ceiling observer is a seam). */
json_t *w31_streamcons_1c03c4_p2_track_preview_ids_from_result(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p2_adapter_prefers_fresh_final @ gateway/stream_consumer_transport.py:StreamTransportMixin._adapter_prefers_fresh_final
 * in {message_id, hook} -> bool.  Needs a real preview and an adapter hook whose
 * answer is literally True; the metadata/chat_id TypeError cascade is a seam, so
 * hook is "absent"/"true"/"false"/"truthy"/"raise" (or either legacy signature). */
int w31_streamcons_1c03c4_p2_adapter_prefers_fresh_final(const json_t *in);

/* PoP: w31_streamcons_1c03c4_p2_hold_ambiguous_first_send @ gateway/stream_consumer_transport.py:StreamTransportMixin._hold_ambiguous_first_send
 * in {text, raw_text, cursor, finalize} -> the state left behind: last_sent_text,
 * delivery_ambiguous (finalize only), preview_uncertain + fallback prefix
 * (preview only, one trailing cursor stripped), edit_supported false,
 * already_sent true, id "__no_edit__". */
json_t *w31_streamcons_1c03c4_p2_hold_ambiguous_first_send(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W31_STREAMCONS_1C03C4_P2_H */
