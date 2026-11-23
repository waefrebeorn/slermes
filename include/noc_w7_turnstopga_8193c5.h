#ifndef NOC_W7_TURNSTOPGA_8193C5_H
#define NOC_W7_TURNSTOPGA_8193C5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: turnstopga_apply @ agent/turn_stop_gates.py:apply_stop_gates
 * Replays one apply_stop_gates call with all lazy-import nudges supplied as
 * explicit seams. Input carries final_msg/messages/final_response plus the
 * scripted nudge strings and counters. Returns verdict, mutated rows and the
 * recorded agent-effect counters. */
struct json_t *turnstopga_apply(const struct json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_TURNSTOPGA_8193C5_H */
