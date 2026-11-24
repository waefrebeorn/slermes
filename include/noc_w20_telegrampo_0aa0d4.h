#ifndef NOC_W20_TELEGRAMPO_0AA0D4_H
#define NOC_W20_TELEGRAMPO_0AA0D4_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w20_telegrampo_0aa0d4_fd_bounded_limits @ plugins/platforms/telegram/telegram_pool.py:fd_bounded_limits
 * `limits` with max_connections capped so 2 x paths pools fit in their 1/4
 * share of the process descriptor budget; the same limits when no cap is
 * needed. `request` carries soft (RLIMIT_NOFILE soft, null = none), paths,
 * max_connections, max_keepalive_connections and keepalive_expiry. The result
 * is `{"capped","max_connections","max_keepalive_connections","keepalive_expiry"}`. */
struct json_t *w20_telegrampo_0aa0d4_fd_bounded_limits(const struct json_t *request);

/* PoP: w20_telegrampo_0aa0d4_lease_ops @ plugins/platforms/telegram/telegram_pool.py:PoolGeneration.retire
 * Replays a PoolGeneration's lease accounting over the op sequence in `ops`:
 * begin / begin_fail / end / retire / grace / aclose. Returns
 * `{"active","retired","closed","inner_closes","grace_armed"}`. */
struct json_t *w20_telegrampo_0aa0d4_lease_ops(const struct json_t *ops);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_TELEGRAMPO_0AA0D4_H */
