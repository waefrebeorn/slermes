/*
 * stream_segments.c — C home for upstream Hermes tui_gateway/stream_segments.py.
 *
 * PoP: w15_streamseg_daa21d_utf16_len @ tui_gateway/stream_segments.py:_utf16_len
 * PoP: w15_streamseg_daa21d_start @ tui_gateway/stream_segments.py:TurnSegments.start
 * PoP: w15_streamseg_daa21d_delta @ tui_gateway/stream_segments.py:TurnSegments.delta
 * PoP: w15_streamseg_daa21d_delta_advance @ tui_gateway/stream_segments.py:TurnSegments.delta
 * PoP: w15_streamseg_daa21d_seal @ tui_gateway/stream_segments.py:TurnSegments.seal
 * PoP: w15_streamseg_daa21d_stamp_reply @ tui_gateway/stream_segments.py:stamp_reply_frame
 * PoP: w15_streamseg_daa21d_stamp_session @ tui_gateway/stream_segments.py:stamp_session_frame
 *
 * Cursor identity is passed explicitly (turn_id/segment/offset) so the port is
 * deterministic: the Python class owns a uuid4() that the oracle pins.
 */
#include "noc_w15_streamseg_daa21d.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

static const char *const SESSION_KEY = "_turn_segments";

long long w15_streamseg_daa21d_utf16_len(const char *text) {
    if (!text) return 0;
    long long n = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        unsigned char b = *p;
        if (b < 0x80) { p++; n += 1; }
        else if ((b & 0xE0) == 0xC0) { if (!p[1]) break; p += 2; n += 1; }
        else if ((b & 0xF0) == 0xE0) { if (!p[1] || !p[2]) break; p += 3; n += 1; }
        else if ((b & 0xF8) == 0xF0) { if (!p[1] || !p[2] || !p[3]) break; p += 4; n += 2; }
        else { p++; n += 1; }
    }
    return n;
}

static json_t *payload_copy(const json_t *p) {
    if (p && p->type == JSON_OBJECT) return json_copy(p);
    return json_object();
}

json_t *w15_streamseg_daa21d_start(const char *turn_id) {
    json_t *o = json_object();
    json_set(o, "turn_id", json_string(turn_id ? turn_id : ""));
    return o;
}

json_t *w15_streamseg_daa21d_delta(const char *turn_id, long long segment, long long offset) {
    json_t *o = json_object();
    json_set(o, "turn_id", json_string(turn_id ? turn_id : ""));
    json_set(o, "segment", json_number((double)segment));
    json_set(o, "offset", json_number((double)offset));
    return o;
}

long long w15_streamseg_daa21d_delta_advance(long long offset, const char *text, int text_is_str) {
    if (text_is_str) return offset + w15_streamseg_daa21d_utf16_len(text);
    return offset;
}

json_t *w15_streamseg_daa21d_seal(const char *turn_id, long long segment) {
    json_t *o = json_object();
    json_set(o, "turn_id", json_string(turn_id ? turn_id : ""));
    json_set(o, "segment", json_number((double)segment));
    return o;
}

json_t *w15_streamseg_daa21d_stamp_reply(int has_segments, const char *turn_id,
                                         long long segment, long long offset,
                                         const char *event, const json_t *payload) {
    int is_obj = payload && payload->type == JSON_OBJECT;
    if (!has_segments) {
        if (is_obj) return json_copy(payload);
        return json_null();
    }
    const char *ev = event ? event : "";
    if (!strcmp(ev, "message.start")) {
        json_t *o = payload_copy(payload);
        json_set(o, "turn_id", json_string(turn_id ? turn_id : ""));
        return o;
    }
    if (!payload || payload->type == JSON_NULL) return json_null();
    if (!strcmp(ev, "message.interim") && is_obj) {
        json_t *o = json_copy(payload);
        json_set(o, "turn_id", json_string(turn_id ? turn_id : ""));
        json_set(o, "segment", json_number((double)segment));
        return o;
    }
    if (!strcmp(ev, "message.delta") && is_obj) {
        const json_t *txt = json_obj_get(payload, "text");
        if (txt && txt->type == JSON_STRING) {
            json_t *o = json_copy(payload);
            json_set(o, "turn_id", json_string(turn_id ? turn_id : ""));
            json_set(o, "segment", json_number((double)segment));
            json_set(o, "offset", json_number((double)offset));
            return o;
        }
    }
    if (is_obj) return json_copy(payload);
    return json_null();
}

json_t *w15_streamseg_daa21d_stamp_session(const struct json_t *session,
                                           const char *event,
                                           const struct json_t *payload) {
    const json_t *sess = (const json_t *)session;
    const json_t *segs = NULL;
    if (sess && sess->type == JSON_OBJECT) segs = json_obj_get(sess, SESSION_KEY);
    int has = segs && segs->type == JSON_OBJECT;
    const char *turn_id = "";
    long long segment = 0, offset = 0;
    if (has) {
        turn_id = json_get_str(segs, "turn_id", "");
        segment = (long long)strtoll(json_get_str(segs, "segment", "0"), NULL, 10);
        offset = (long long)strtoll(json_get_str(segs, "offset", "0"), NULL, 10);
    }
    return w15_streamseg_daa21d_stamp_reply(has, turn_id, segment, offset, event,
                                            (const json_t *)payload);
}
