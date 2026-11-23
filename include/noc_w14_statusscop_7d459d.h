#ifndef NOC_W14_STATUSSCOP_7D459D_H
#define NOC_W14_STATUSSCOP_7D459D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_statusscop_7d459d_same_owner @ gateway/status_scoped_locks.py:_same_owner
 *
 * Faithful port of _same_owner(existing, holder) together with the two
 * gateway.status helpers it reads at call time (_pid_from_record and
 * _start_times_conflict). Returns a JSON boolean. Caller frees.
 * The process-probing / filesystem surfaces of the module are excluded; only
 * this pure ownership comparison is gated. */
struct json_t *w14_statusscop_7d459d_same_owner(const struct json_t *existing,
                                                const struct json_t *holder);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_STATUSSCOP_7D459D_H */
