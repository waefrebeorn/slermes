#ifndef NOC_W8_SESSIONEVEN_E26544_H
#define NOC_W8_SESSIONEVEN_E26544_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_sessioneven_e26544_note_progress @ gateway/session_events.py:SessionEvents.note_progress
 * Replays one execution frame against an optional _progress state (JSON object or
 * JSON null for "no progress yet") and returns the resulting _progress state.
 * `now` is injected for time.time() (the caller holds the lock, generation fence
 * is checked by the caller in Python; here the generation is replayed faithfully). */
struct json_t *w8_sessioneven_e26544_note_progress(const struct json_t *state,
                                                    const struct json_t *generation,
                                                    const char *event_type,
                                                    const struct json_t *payload,
                                                    double now);

/* PoP: w8_sessioneven_e26544_progress_for @ gateway/session_events.py:SessionEvents.progress_for
 * Returns {"progress_at": at, "tool": <newest open tool>} or {"progress_at": at}
 * or {} for a stale/missing generation. */
struct json_t *w8_sessioneven_e26544_progress_for(const struct json_t *state,
                                                  const struct json_t *generation);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_SESSIONEVEN_E26544_H */
