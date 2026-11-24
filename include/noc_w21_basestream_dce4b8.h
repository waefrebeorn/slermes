#ifndef NOC_W21_BASESTREAM_DCE4B8_H
#define NOC_W21_BASESTREAM_DCE4B8_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/platforms/base_streaming_tts.py (gate w21_basestream_dce4b8).
 *
 * The module is pure stdlib (two dataclasses and two turn-scoped suppression
 * helpers), so the whole descriptor / handle / suppression surface is ported.
 * A dataclass instance travels as a JSON object of field -> value.
 */

/* PoP: w21_basestream_dce4b8_audio_format @ gateway/platforms/base_streaming_tts.py:AudioFormat
 * `AudioFormat(**overrides)` as a field dict: sample_rate=24000, channels=1,
 * sample_width=2, overridden one-for-one. Unknown kwarg ->
 *   {"status":"error","type":"TypeError","message":"AudioFormat.__init__() ..."}
 * else {"status":"ok","value":{...}}. NULL overrides reads as no overrides. */
json_t *w21_basestream_dce4b8_audio_format(const json_t *overrides);

/* PoP: w21_basestream_dce4b8_handle @ gateway/platforms/base_streaming_tts.py:StreamingTTSHandle
 * `StreamingTTSHandle(**overrides)` flattened to
 *   {chat_id, audio_format:{sample_rate,channels,sample_width}, audible, aborted}
 * with chat_id="", audible=false, aborted=false by default. */
json_t *w21_basestream_dce4b8_handle(const json_t *overrides);

/* PoP: w21_basestream_dce4b8_turn_key @ gateway/platforms/base_streaming_tts.py:streaming_tts_turn_key
 * `f"{session_key}:{turn_marker}"`; NULL when session_key is falsy or the
 * marker stays None. With a None marker and an event, the marker is the event's
 * truthy message_id, else its truthy platform_update_id. Caller frees. */
char *w21_basestream_dce4b8_turn_key(const json_t *session_key, const json_t *turn_marker,
                                    const json_t *event);

/* PoP: w21_basestream_dce4b8_should_skip @ gateway/platforms/base_streaming_tts.py:streaming_tts_should_skip_whole_file
 * `bool(turn_key and turn_key in completed_turns)`; completed_turns is the JSON
 * array of strings (Python's set[str]). */
int w21_basestream_dce4b8_should_skip(const json_t *completed_turns, const json_t *session_key,
                                     const json_t *turn_marker, const json_t *event);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_BASESTREAM_DCE4B8_H */
