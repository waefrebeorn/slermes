#ifndef NOC_W6_SLEEPCLOCK_919D0B_H
#define NOC_W6_SLEEPCLOCK_919D0B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_sleepclock_919d0b_sleep_threshold @ agent/sleep_clock.py:_SLEEP_THRESHOLD_S
 * The minimum gap (seconds) that counts as a sleep; smaller gaps are noise. */
double w6_sleepclock_919d0b_sleep_threshold(void);

/* PoP: w6_sleepclock_919d0b_slept @ agent/sleep_clock.py:SleepWatch.slept
 * Pure seam: `prev_wall` stands in for self._wall at the previous check,
 * `wall` for time.time() at this check.  Returns wall - prev_wall - interval
 * when that value is >= the sleep threshold, else 0.0 (mirrors the
 * `stopped if stopped >= _SLEEP_THRESHOLD_S else 0.0` guard). */
double w6_sleepclock_919d0b_slept(double prev_wall, double wall, double interval);

/* PoP: w6_sleepclock_919d0b_slept_sequence @ agent/sleep_clock.py:SleepWatch.slept
 * Runs slept() across a sequence of wall readings with one shared watch:
 * each reading updates the watch's previous-wall value, exactly as the
 * Python `stopped, self._wall = ..., wall` assignment does.  `walls` must be
 * a JSON array of numbers; returns a NEW JSON array of numbers (caller frees). */
struct json_t *w6_sleepclock_919d0b_slept_sequence(double interval, double initial_wall,
                                                   const struct json_t *walls);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_SLEEPCLOCK_919D0B_H */
