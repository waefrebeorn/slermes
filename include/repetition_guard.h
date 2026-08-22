/*
 * repetition_guard.h — C11 port of agent/repetition_guard.py
 *
 * Cheap content-sanity checks for the truncated-response continuation path
 * (issue #86581). Python name -> C symbol mapping:
 *
 *   is_repetition_dominated(text)      -> repetition_guard_is_repetition_dominated
 *   _line_repetition_dominated(text,n)  -> (static, internal)
 *
 * NULL text fails open (returns 0), mirroring Python's non-str branch.
 */

#ifndef HERMES_REPETITION_GUARD_H
#define HERMES_REPETITION_GUARD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: MIN_FRAGMENT_LENGTH @ agent/repetition_guard.py */
#define REPETITION_GUARD_MIN_FRAGMENT_LENGTH 400
/* PoP: _REPEAT_WINDOW @ agent/repetition_guard.py */
#define REPETITION_GUARD_REPEAT_WINDOW 60
/* PoP: _MIN_REPEAT_COUNT @ agent/repetition_guard.py */
#define REPETITION_GUARD_MIN_REPEAT_COUNT 5
/* PoP: _DOMINANCE_RATIO @ agent/repetition_guard.py */
#define REPETITION_GUARD_DOMINANCE_RATIO 0.5

/* PoP: is_repetition_dominated @ agent/repetition_guard.py:is_repetition_dominated
 * True (1) when `text` is dominated by verbatim repeated fragments.
 * Returns 0 for NULL / short inputs (fail-open). */
int repetition_guard_is_repetition_dominated(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_REPETITION_GUARD_H */
