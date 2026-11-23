#ifndef NOC_W12_STUCKLOOP_9D2C5D_H
#define NOC_W12_STUCKLOOP_9D2C5D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_stuckloop_9d2c5d_increment @ gateway/stuck_loop_counts.py:increment_restart_failure_counts
 * {key: counts.get(key, 0) + 1 for key in active_session_keys} (set semantics: keys deduped). */
struct json_t *w12_stuckloop_9d2c5d_increment(const struct json_t *counts,
                                              const struct json_t *keys);

/* PoP: w12_stuckloop_9d2c5d_clear @ gateway/stuck_loop_counts.py:clear_restart_failure_count
 * Returns {"counts":..., "changed":bool, "unlink":bool}; unlink means the file is removed. */
struct json_t *w12_stuckloop_9d2c5d_clear(const struct json_t *counts,
                                          const char *session_key);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_STUCKLOOP_9D2C5D_H */
