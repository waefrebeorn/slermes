/*
 * noc_w33_telegramne_429779_p1.h - C surface for the request-shaping leaves of
 * upstream plugins/platforms/telegram/telegram_network.py (gate
 * w33_telegramne_429779_p1).
 *
 * Ported: the HERMES_TELEGRAM_HTTP_* casts, the PTB HTTPXRequest sizing, the
 * per-platform getUpdates keepalive count, the SO_KEEPALIVE option set, the
 * NO_PROXY target-host list, the Bot API base_url normaliser, the
 * tls_classic_key_exchange predicate and the https-proxy classic-TLS
 * predicate.  os.environ, sys.platform, the socket option table, the platform
 * httpx Limits and the live SSL/Proxy identities are seams: they arrive as
 * fixture inputs.
 */
#ifndef NOC_W33_TELEGRAMNE_429779_P1_H
#define NOC_W33_TELEGRAMNE_429779_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {key, env: {str: str}, default, cast: "int"|"float"} -> cast(env[key]) or default. */
json_t *w33_telegramne_429779_env_number(const json_t *in);

/* in: {env} -> {connection_pool_size, pool_timeout, connect_timeout,
 * read_timeout, write_timeout, media_write_timeout}. */
json_t *w33_telegramne_429779_ptb_request_kwargs(const json_t *in);

/* in: {platform} -> 0 on win32, else 1. */
json_t *w33_telegramne_429779_getupdates_keepalive(const json_t *in);

/* in: {avail: [socket option names]} -> [[family, option, value], ...]. */
json_t *w33_telegramne_429779_keepalive_options(const json_t *in);

/* in: {base_url, fallback_ips} -> ["host:port"] for the base_url, else
 * ["api.telegram.org", *fallback_ips]. */
json_t *w33_telegramne_429779_proxy_target_hosts(const json_t *in);

/* in: {extra} -> {} or {base_url, base_file_url} with the Bot API path added
 * to a bare server URL. */
json_t *w33_telegramne_429779_bot_api_urls(const json_t *in);

/* in: {extra} -> {result: "default"|"classic", warned: bool}. */
json_t *w33_telegramne_429779_verify_decision(const json_t *in);

/* in: {proxy_url, is_str, verify_is_classic} -> {result: "wrap"|"passthrough"}. */
json_t *w33_telegramne_429779_classic_proxy_decision(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
