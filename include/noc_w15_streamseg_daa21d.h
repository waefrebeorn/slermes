#ifndef NOC_W15_STREAMSEG_DAA21D_H
#define NOC_W15_STREAMSEG_DAA21D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_streamseg_daa21d_utf16_len @ tui_gateway/stream_segments.py:_utf16_len
 * UTF-16 code-unit count of a UTF-8 string (astral code points count 2). */
long long w15_streamseg_daa21d_utf16_len(const char *text);

/* PoP: w15_streamseg_daa21d_start @ tui_gateway/stream_segments.py:TurnSegments.start
 * Returns {"turn_id": turn_id}. Caller frees. */
struct json_t *w15_streamseg_daa21d_start(const char *turn_id);

/* PoP: w15_streamseg_daa21d_delta @ tui_gateway/stream_segments.py:TurnSegments.delta
 * Returns {"turn_id","segment","offset"} for the current cursor. Caller frees. */
struct json_t *w15_streamseg_daa21d_delta(const char *turn_id, long long segment,
                                          long long offset);

/* PoP: w15_streamseg_daa21d_delta_advance @ tui_gateway/stream_segments.py:TurnSegments.delta
 * The cursor offset after a delta: advances by _utf16_len(text) only for a str text. */
long long w15_streamseg_daa21d_delta_advance(long long offset, const char *text,
                                              int text_is_str);

/* PoP: w15_streamseg_daa21d_seal @ tui_gateway/stream_segments.py:TurnSegments.seal
 * Returns {"turn_id","segment"} for the current cursor. Caller frees. */
struct json_t *w15_streamseg_daa21d_seal(const char *turn_id, long long segment);

/* PoP: w15_streamseg_daa21d_stamp_reply @ tui_gateway/stream_segments.py:stamp_reply_frame
 * Payload (object/null) stamped with the segment identity for the given event. Caller frees. */
struct json_t *w15_streamseg_daa21d_stamp_reply(int has_segments, const char *turn_id,
                                                long long segment, long long offset,
                                                const char *event, const struct json_t *payload);

/* PoP: w15_streamseg_daa21d_stamp_session @ tui_gateway/stream_segments.py:stamp_session_frame
 * stamp_reply_frame using session["***"] as the cursor, if present. Caller frees. */
struct json_t *w15_streamseg_daa21d_stamp_session(const struct json_t *session,
                                                  const char *event,
                                                  const struct json_t *payload);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_STREAMSEG_DAA21D_H */
