/* noc_w19_voicementi_2db922.h - public C API for plugins/platforms/matrix/voice_mention.py port. */
#ifndef NOC_W19_VOICEMENTI_2DB922_H
#define NOC_W19_VOICEMENTI_2DB922_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_voicementi_2db922_has_voice_marker @ plugins/platforms/matrix/voice_mention.py:has_voice_marker
 * The single MSC3245 voice-message check: content.get("org.matrix.msc3245.voice") is not None. */
int w19_voicementi_2db922_has_voice_marker(json_t *content);

/* PoP: w19_voicementi_2db922_is_voice_event @ plugins/platforms/matrix/voice_mention.py:is_voice_event
 * msgtype == "m.audio" and the MSC3245 voice marker. */
int w19_voicementi_2db922_is_voice_event(json_t *content);

/* PoP: w19_voicementi_2db922_parked_replay @ plugins/platforms/matrix/voice_mention.py:ParkedVoices
 * Deterministic replay of one ParkedVoices instance: `ops` is an array of
 * {"op":"mark|begin|release|park|claim|pending|prune", "room","sender","now", ...}.
 * Every op carries the monotonic `now` the call would see (the clock seam), so the
 * claim window, the park ordering, the floor and the per-sender cap are exercised
 * without a wall clock. Answers one trace array, one entry per op (claim answers
 * [event_id, content, relates_to] or null). */
json_t *w19_voicementi_2db922_parked_replay(json_t *ops);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_VOICEMENTI_2DB922_H */
