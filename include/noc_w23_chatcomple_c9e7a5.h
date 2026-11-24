/*
 * noc_w23_chatcomple_c9e7a5.h - C surface for upstream
 * agent/chat_completion_wait_notice.py (gate w23_chatcomple_c9e7a5).
 *
 * Operator-facing wait notice for long provider silences: the status line text
 * for a wait phase, the earliest enabled Codex watchdog deadline, and the
 * per-request memory of what the line currently shows.
 *
 * Seams: none inside the module.  The harness supplies only the arguments the
 * caller holds (phase, silence, watchdog tuple, the clock readings) and the
 * "infinity" flag that Python expresses as math.inf and JSON cannot.
 */
#ifndef NOC_W23_CHATCOMPLE_C9E7A5_H
#define NOC_W23_CHATCOMPLE_C9E7A5_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W23_CHATCOMPLE_C9E7A5_NEAR_DEADLINE_SECS 15.0

/* world keys: model, silence_secs, phase, watchdog [label, seconds] | null.
 * Returns the status line (UTF-8).  Caller frees. */
char *w23_chatcomple_c9e7a5_wait_notice_text(const json_t *world);

/* world keys: stale_timeout, stale_finite, ttfb_enabled, ttfb_timeout,
 * ttfb_finite, last_event_ts, last_progress_ts, retry_started_ts, call_start,
 * idle_enabled, idle_timeout, idle_finite, idle_requires_progress, elapsed,
 * progress_timeout.  Returns [label, seconds_until_it_fires] or null. */
json_t *w23_chatcomple_c9e7a5_codex_watchdog_deadline(const json_t *world);

/* world key steps[]: {phase, watchdog [label, seconds] | null, reset}.  Returns
 * one bool per non-reset step: would the status line be rewritten? */
json_t *w23_chatcomple_c9e7a5_should_emit(const json_t *world);

/* world key watchdog [label, seconds] | null -> within NEAR_DEADLINE_SECS. */
int w23_chatcomple_c9e7a5_near_deadline(const json_t *world);

/* _PHASE_TEXT keys in dict order. */
json_t *w23_chatcomple_c9e7a5_phases(void);

double w23_chatcomple_c9e7a5_near_secs(void);

#ifdef __cplusplus
}
#endif
#endif
