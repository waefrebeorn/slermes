#ifndef PORT_TURN_LIVENESS_H
#define PORT_TURN_LIVENESS_H

#include <stddef.h>

/* Port of Python: agent/turn_liveness.py — the PURE settings half.
 *
 * The Python module also owns TurnLivenessWatchdog, a thread-sampling state
 * machine bound to a live agent object (activity lock, stop event, commit
 * callback, scheduler). No C caller exists for a turn watchdog today, so that
 * half stays unported rather than being faked with an empty body.
 */

#define TL_DEFAULT_TIMEOUT_S 600.0
#define TL_DEFAULT_POLL_S    15.0
#define TL_MIN_POLL_S        0.01

/* Coerce one duration knob: rejects typos, NaN and Inf; never raises.
 * `raw` is the textual config value (NULL = key absent -> default).
 * Returns 1 when the value was invalid (a warning is due), 0 otherwise.
 * *out always receives a finite value. */
int turn_liveness_resolve_finite_seconds(const char *raw, double def,
                                         double *out);

/* The exact warning body Python logs:
 *   "Invalid %s in config.yaml: %r — falling back to default %.1f."
 * Returns a malloc'd string (caller frees). */
char *turn_liveness_invalid_value_message(const char *key, const char *raw,
                                          double def);

/* Resolve (timeout_s, poll_s) from the config document.
 *
 * `config_json` may be NULL (Python's config=None) or a JSON object.
 * *has_timeout is 0 when the timeout is opted out (timeout_s <= 0 -> None);
 * otherwise *timeout_s holds the value. *poll_s is always > 0.
 * Returns the number of warnings the resolution emitted (0..2). */
int turn_liveness_resolve_settings(const char *config_json, int *has_timeout,
                                   double *timeout_s, double *poll_s);

#endif /* PORT_TURN_LIVENESS_H */
