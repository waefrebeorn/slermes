/*
 * turn_overflow.c — C home for upstream Hermes agent/turn_overflow.py.
 * Ported behavior: the pure output-cap clamp in _clamp_output_cap. The rest of
 * the recovery handlers mutate the agent / serialize messages / sleep and stay
 * in Python.
 * PoP: w6_turnoverflo_clamp_output_cap @ agent/turn_overflow.py:_clamp_output_cap
 */
#include "noc_w6_turnoverflo_10ab04.h"

long long w6_turnoverflo_clamp_output_cap(
    long long available_out, long long old_ctx, long long request_input_estimate)
{
    long long local_available_out = old_ctx - request_input_estimate;
    long long safe_out;
    if (local_available_out > 0) {
        long long smaller = available_out < local_available_out
                            ? available_out : local_available_out;
        safe_out = smaller - 64;
    } else {
        /* Local estimate can overshoot; fall back to the provider budget. */
        safe_out = available_out - 64;
    }
    return safe_out > 1 ? safe_out : 1; /* Python max(1, safe_out) */
}
