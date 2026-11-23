#ifndef NOC_W8_AUTHCODEX_1AA766_H
#define NOC_W8_AUTHCODEX_1AA766_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Faithful C11 port of the pure helpers in hermes_cli/auth_codex.py. */

/* PoP: w8_authcodex_1aa766_stripped @ hermes_cli/auth_codex.py:_stripped */
struct json_t *w8_authcodex_1aa766_stripped(const struct json_t *value);

/* PoP: w8_authcodex_1aa766_is_codex_rate_limit_shaped @ hermes_cli/auth_codex.py:_is_codex_rate_limit_shaped */
struct json_t *w8_authcodex_1aa766_is_codex_rate_limit_shaped(const struct json_t *code,
        const struct json_t *reason, const struct json_t *message);

/* PoP: w8_authcodex_1aa766_entry_is_rate_limit_exhausted @ hermes_cli/auth_codex.py:_entry_is_rate_limit_exhausted */
struct json_t *w8_authcodex_1aa766_entry_is_rate_limit_exhausted(const struct json_t *entry);

/* PoP: w8_authcodex_1aa766_codex_usage_probe_url @ hermes_cli/auth_codex.py:_codex_usage_probe_url */
struct json_t *w8_authcodex_1aa766_codex_usage_probe_url(const struct json_t *base_url);

/* PoP: w8_authcodex_1aa766_codex_quota_probe_cache_key @ hermes_cli/auth_codex.py:_codex_quota_probe_cache_key */
struct json_t *w8_authcodex_1aa766_codex_quota_probe_cache_key(const char *token);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_AUTHCODEX_1AA766_H */
