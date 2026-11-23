/* noc_w11_eventrepla_0d019f.h — public C API for the faithful port of
 * upstream Hermes tui_gateway/event_replay.py.
 *
 * PoP: w11_eventrepla_0d019f_batch @ tui_gateway/event_replay.py:events_since
 * PoP: w11_eventrepla_0d019f_stamp @ tui_gateway/event_replay.py:_stamp_event
 */
#ifndef NOC_W11_EVENTREPLA_0D019F_H
#define NOC_W11_EVENTREPLA_0D019F_H

#include "hermes_json.h"

/* Reset all replay state (Python reset_replay_state). */
void w11_eventrepla_0d019f_reset(void);

/* Test seam: override the four module-level caps (Python module constants).
 * max_events, max_sessions, max_bytes_session, max_bytes_process. */
void w11_eventrepla_0d019f_configure(long long max_events, long long max_sessions,
                                     long long max_bytes_session, long long max_bytes_process);

/* Stamp one outgoing event frame. Mirrors _stamp_event: mutates event["params"]["seq"]
 * and returns the assigned sequence number (0 when the frame is not an event
 * bearing a non-empty session_id). */
long long w11_eventrepla_0d019f_stamp(json_t *event);

/* Recorded event objects (params dicts, with seq restored) with seq > last_seen. */
json_t *w11_eventrepla_0d019f_events_since(const char *sid, long long last_seen);

/* True when events in (last_seen, oldest retained] were evicted. */
bool w11_eventrepla_0d019f_is_truncated(const char *sid, long long last_seen);

/* Highest stamped seq for sid, or 0 when unknown. */
long long w11_eventrepla_0d019f_latest_seq(const char *sid);

/* Release a torn-down session's ring (keeps monotonic seq counters). */
void w11_eventrepla_0d019f_forget(const char *sid);

/* Purge everything recorded under a private key (ring AND counters). */
void w11_eventrepla_0d019f_release(const char *key);

/* Telemetry snapshot. */
json_t *w11_eventrepla_0d019f_stats(void);

/* Scripted batch driver. ops is a JSON array of [name, ...args] entries:
 *   ["reset"] | ["config",e,s,bs,bp] | ["stamp",event] | ["events_since",sid,last]
 *   ["is_truncated",sid,last] | ["latest_seq",sid] | ["forget",sid]
 *   ["release",key] | ["stats"]
 * Returns a JSON array with one result per op. */
json_t *w11_eventrepla_0d019f_batch(json_t *ops);

#endif /* NOC_W11_EVENTREPLA_0D019F_H */
