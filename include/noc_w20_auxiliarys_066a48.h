#ifndef NOC_W20_AUXILIARYS_066A48_H
#define NOC_W20_AUXILIARYS_066A48_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/auxiliary_stream_watchdog.py (gate w20_auxiliarys_066a48).
 *
 * The window arithmetic, the re-arm/fire state machine and the two timeout
 * messages are pure; the socket pinning, the fenced release and the real
 * threading.Timer stay upstream behind these seams:
 *   - `local` / `cloud_stale` are the answers of waits_like_local() and
 *     _cloud_stale_timeout_for(), injected per call instead of reading config;
 *   - `now` / event clocks are injected instead of awake_seconds();
 *   - scheduling is counted (generation) instead of starting a thread.
 */

/* PoP: w20_auxiliarys_066a48_stream_windows @ agent/auxiliary_stream_watchdog.py:chat_stream_windows
 * spec = {"task_timeout": number|null, "aux_default": number,
 *         "cloud_stale": number, "local": bool, "timeout": number|null}
 * -> [inter-chunk window, first-token window|null]
 *   window = task_timeout or aux_default
 *   first  = null when local else max(window, cloud_stale)
 *   a positive numeric timeout caps both (bool counts as 1/0, like Python int). */
json_t *w20_auxiliarys_066a48_stream_windows(const json_t *spec);

/* PoP: w20_auxiliarys_066a48_timeout_error @ agent/auxiliary_stream_watchdog.py:ChatStreamWatchdog.timeout_error
 * The two TimeoutError messages; `first_token_window` null reads as 0. */
char *w20_auxiliarys_066a48_timeout_error(int saw_progress, double window,
                                          const json_t *first_token_window, double elapsed);

/* PoP: w20_auxiliarys_066a48_close_chunk_stream @ agent/auxiliary_stream_watchdog.py:_close_chunk_stream
 * spec = {"has_close": bool, "has_aclose": bool, "allow_aclose": bool,
 *         "raises": bool, "awaitable": bool}
 * -> null | "awaitable"  (the value is always None unless close() returned
 * an awaitable). */
const char *w20_auxiliarys_066a48_close_chunk_stream(const json_t *spec);

/* PoP: w20_auxiliarys_066a48_simulate @ agent/auxiliary_stream_watchdog.py:ChatStreamWatchdog
 * Runs the constructor / _rearm / progress / _fire / finish / timeout_error
 * sequence against an injected clock (no socket, no thread):
 *   spec = {"window": n, "first_token": n|null, "start": n,
 *           "progress_at": n|null, "fire_at": n|null, "stale_generation": bool}
 * -> {"saw_progress","fired","done","deadline_offset":n|null,"generation":n,"error":s}
 * `generation` counts _schedule() calls, which is exactly what the real
 * _generation counter records (the timer is what is stubbed out). */
json_t *w20_auxiliarys_066a48_simulate(const json_t *spec);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_AUXILIARYS_066A48_H */
