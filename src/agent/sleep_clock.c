/*
 * sleep_clock.c — C home for upstream Hermes agent/sleep_clock.py.
 *
 * PoP: w6_sleepclock_919d0b_sleep_threshold @ agent/sleep_clock.py:_SLEEP_THRESHOLD_S
 * PoP: w6_sleepclock_919d0b_slept @ agent/sleep_clock.py:SleepWatch.slept
 * PoP: w6_sleepclock_919d0b_slept_sequence @ agent/sleep_clock.py:SleepWatch.slept
 *
 * The clock source (Windows QueryUnbiasedInterruptTime vs time.monotonic) is
 * platform plumbing and is intentionally not reimplemented here; the tested
 * behaviour is the pure SleepWatch.slept() arithmetic, driven by injected wall
 * readings so it is deterministic and identical on every host.
 */
#include "noc_w6_sleepclock_919d0b.h"
#include "hermes_json.h"

#include <stddef.h>

#define W6_SLEEPCLOCK_THRESHOLD_S 5.0

double w6_sleepclock_919d0b_sleep_threshold(void)
{
    return W6_SLEEPCLOCK_THRESHOLD_S;
}

double w6_sleepclock_919d0b_slept(double prev_wall, double wall, double interval)
{
    double stopped = wall - prev_wall - interval;
    return stopped >= W6_SLEEPCLOCK_THRESHOLD_S ? stopped : 0.0;
}

struct json_t *w6_sleepclock_919d0b_slept_sequence(double interval, double initial_wall,
                                                   const struct json_t *walls)
{
    json_t *out = json_array();
    double prev = initial_wall;
    size_t n = (walls && walls->type == JSON_ARRAY) ? json_len(walls) : 0;

    for (size_t i = 0; i < n; i++) {
        json_t *w = json_get(walls, i);
        double wall = (w && w->type == JSON_NUMBER) ? w->num_val : 0.0;
        double stopped = wall - prev - interval;
        json_append(out, json_number(stopped >= W6_SLEEPCLOCK_THRESHOLD_S ? stopped : 0.0));
        prev = wall;  /* self._wall = wall on every call, even below threshold */
    }
    return out;
}
