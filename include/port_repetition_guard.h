#ifndef PORT_REPETITION_GUARD_H
#define PORT_REPETITION_GUARD_H

/* Port of Python: agent/repetition_guard.py — model-output repetition sanity.
 *
 * All six public checks are pure and ported. The RunawayStreamWatch class is
 * streaming runtime state (a live channel that carries a growing tail) and is
 * the only part left to C; its `is_runaway_repetition` check on a window is
 * here.
 */

#include <stddef.h>

/* The interrupt message shown when a repetition-dominated partial is dropped. */
#define RG_REPETITION_LOOP_INTERRUPTED \
    "[the reply degenerated into a repetition loop and was interrupted]"

/* True when a contiguous run of >=5 exact repetitions covers >= half of text
 * (len >= 400 chars, computed as in Python). Fail-open for short/non-string. */
int rg_is_repetition_dominated(const char *text);

/* One 60-char window recurs often enough to cover half of `n` chars. */
int rg_window_count_dominated(const char *text, int n);

/* Dominant exact periodic run recovered from evenly spaced 60-char anchors. */
int rg_periodic_run_dominated(const char *text, int n);

/* Expand one equal window to [left, right) of its exact periodic run. */
void rg_expand_run(const char *text, int n, int start, int period, int matched,
                   int *left, int *right);

/* Stricter: repetition-dominated AND (noise lines few) -> a runaway loop. */
int rg_is_runaway_repetition(const char *text);

/* True when a single normalized line covers half of `n` chars via repeats. */
int rg_line_repetition_dominated(const char *text, int n);

#endif /* PORT_REPETITION_GUARD_H */
