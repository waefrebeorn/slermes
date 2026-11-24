/*
 * noc_w33_telegramne_429779_p2.h - C surface for the fallback-path leaves of
 * upstream plugins/platforms/telegram/telegram_network.py (gate
 * w33_telegramne_429779_p2).
 *
 * Ported: the IPv4 fallback sanity filter and its FD cap, the env list parser,
 * the retryable-connect classification, the sticky attempt order and its
 * connect-budget split, the per-request connect narrowing, the host-preserving
 * IP rewrite, the DoH answer filter, the discovery merge, the PTB pool/limits
 * derivation and the walk-failure diagnostic.  DNS sockets, the DoH HTTP
 * client, RLIMIT_NOFILE, time.monotonic and the proxy/SSL identities are
 * seams fed as fixture inputs.
 */
#ifndef NOC_W33_TELEGRAMNE_429779_P2_H
#define NOC_W33_TELEGRAMNE_429779_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {values: [any]} -> the public IPv4 literals kept, in order. */
json_t *w33_telegramne_429779_normalize_fallback_ips(const json_t *in);

/* in: {values} -> normalized, deduped, at most 4 non-seed IPs; seeds never
 * count toward the cap. */
json_t *w33_telegramne_429779_cap_fallback_ips(const json_t *in);

/* in: {value: "ip,ip,..."|null} -> the valid public IPv4 literals. */
json_t *w33_telegramne_429779_parse_fallback_ip_env(const json_t *in);

/* in: {mro: [class names]} -> bool (ConnectTimeout / ConnectError anywhere
 * in the MRO). */
json_t *w33_telegramne_429779_is_retryable(const json_t *in);

/* in: {sticky: {set, value}, fallback_ips} -> sticky first, then the IPs, then
 * null (the dual-stack hostname) last. */
json_t *w33_telegramne_429779_attempt_order(const json_t *in);

/* in: {budget, elapsed, paths, index} -> this path's share of the walk budget. */
json_t *w33_telegramne_429779_connect_share(const json_t *in);

/* in: {ext: "dict"|"none"|"other", connect: number|null, budget} ->
 * {changed, connect}. */
json_t *w33_telegramne_429779_request_connect_budget(const json_t *in);

/* in: {url, ip, host} -> {url, host_header, sni} with the host preserved. */
json_t *w33_telegramne_429779_rewrite_for_ip(const json_t *in);

/* in: {body, status} -> the A-record IPs from a DoH JSON answer. */
json_t *w33_telegramne_429779_doh_answer_ips(const json_t *in);

/* in: {doh_ips, seeds} -> validated DoH IPs followed by the seeds. */
json_t *w33_telegramne_429779_discover_merge(const json_t *in);

/* in: {fallback_ips, proxy_url, env, platform, soft, base} ->
 * {kwargs, ips, pool, updates, route}. */
json_t *w33_telegramne_429779_pool_plan(const json_t *in);

/* in: {failures: [[path, failure]], attempted, order_len} ->
 * {reason, message}. */
json_t *w33_telegramne_429779_walk_failed(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
