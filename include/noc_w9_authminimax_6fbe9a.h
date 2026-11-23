#ifndef NOC_W9_AUTHMINIMAX_6FBE9A_H
#define NOC_W9_AUTHMINIMAX_6FBE9A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_authminimax_6fbe9a_expired_in_looks_like_unix_ms @ hermes_cli/auth_minimax.py:_minimax_expired_in_looks_like_unix_ms
 * int(expired_in) > (now_ms // 2). */
int w9_authminimax_6fbe9a_expired_in_looks_like_unix_ms(const struct json_t *expired_in,
                                                       const struct json_t *now_ms);

/* PoP: w9_authminimax_6fbe9a_resolve_token_expiry_unix @ hermes_cli/auth_minimax.py:_minimax_resolve_token_expiry_unix
 * unix-ms absolute (raw/1000) vs TTL seconds (now + max(1,raw)). now_seconds is injected. */
double w9_authminimax_6fbe9a_resolve_token_expiry_unix(const struct json_t *expired_in,
                                                       double now_seconds);

/* PoP: w9_authminimax_6fbe9a_expiry_fields @ hermes_cli/auth_minimax.py:_minimax_expiry_fields
 * {obtained_at, expires_at, expires_in}; now_seconds injected for a deterministic clock. */
struct json_t *w9_authminimax_6fbe9a_expiry_fields(const struct json_t *expired_in,
                                                   double now_seconds);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_AUTHMINIMAX_6FBE9A_H */
