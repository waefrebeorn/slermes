#ifndef HERMES_LOOPS_PURE_H
#define HERMES_LOOPS_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic helpers ported from hermes_cli/loops.py.
 * All return freshly malloc'd strings (free with free()) or NULL. */

int  loops_parse_interval_token(const char *token);
char *loops_format_interval(double seconds);
int  loops_response_signals_complete(const char *response);

int  loops_min_interval_seconds(const char *config_json);
int  loops_max_ticks_default(const char *config_json);
int  loops_self_paced_floor_seconds(const char *config_json);
int  loops_self_paced_ceiling_seconds(const char *config_json);

char *loops_parse_loop_args(const char *text);

/* LoopState (loops.py::LoopState) — opaque. Construct via from_json. */
typedef struct loop_state loop_state_t;

/* PoP: loops_state_from_json @ hermes_cli/loops.py:LoopState.from_json */
loop_state_t *loops_state_from_json(const char *raw);
/* PoP: loops_state_cadence_label @ hermes_cli/loops.py:LoopState.cadence_label */
char *loops_state_cadence_label(const loop_state_t *s);
/* PoP: loops_state_remaining_label @ hermes_cli/loops.py:LoopState.remaining_label
 * `now` is injected (Python uses time.time()) for deterministic output. */
char *loops_state_remaining_label(const loop_state_t *s, double now);
/* PoP: loops_state_is_due @ hermes_cli/loops.py:LoopManager.is_due */
int loops_state_is_due(const loop_state_t *s, double now);
/* PoP: loops_state_status_line @ hermes_cli/loops.py:LoopManager.status_line
 * `now` injected for deterministic remaining-label computation. */
char *loops_state_status_line(const loop_state_t *s, double now);

/* PoP: loops_digest_response @ hermes_cli/loops.py:_digest_response
 * Returns malloc'd lowercase SHA-256 hex digest (free with free()). */
char *loops_digest_response(const char *response);

void loops_state_free(loop_state_t *s);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_LOOPS_PURE_H */
