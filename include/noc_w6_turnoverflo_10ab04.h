#ifndef NOC_W6_TURNOVERFLO_10AB04_H
#define NOC_W6_TURNOVERFLO_10AB04_H
#include <stddef.h>

/* C home for upstream Hermes agent/turn_overflow.py (gate w6_turnoverflo_10ab04).
 * The loop handlers themselves are agent/I/O driven; the pure numeric decision
 * inside _clamp_output_cap (the authoritative retry max_tokens bound) is ported. */

/* PoP: w6_turnoverflo_clamp_output_cap @ agent/turn_overflow.py:_clamp_output_cap
 * Mirrors: local = old_ctx - request_input_estimate;
 *          safe  = max(1, min(available_out, local) - 64) if local > 0
 *                  else max(1, available_out - 64). */
long long w6_turnoverflo_clamp_output_cap(
    long long available_out, long long old_ctx, long long request_input_estimate);

#endif /* NOC_W6_TURNOVERFLO_10AB04_H */
