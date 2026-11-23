#ifndef NOC_W7_AUTHQWEN_4B7464_H
#define NOC_W7_AUTHQWEN_4B7464_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_authqwen_4b7464_access_token_is_expiring @ hermes_cli/auth_qwen.py:_qwen_access_token_is_expiring
 * Faithful port of `int(expiry_date_ms)` except handling (invalid -> True),
 * `max(0, int(skew_seconds))`, and `(now + skew) * 1000 >= expiry_ms`.
 * now_seconds is injected so the oracle is deterministic. */
int w7_authqwen_4b7464_access_token_is_expiring(const struct json_t *expiry_date_ms,
                                                const struct json_t *skew_seconds,
                                                double now_seconds);

/* PoP: w7_authqwen_4b7464_qwen_base_url @ hermes_cli/auth_qwen.py:_qwen_base_url
 * env_value is the injected value of os.getenv("HERMES_QWEN_BASE_URL", ""). */
struct json_t *w7_authqwen_4b7464_qwen_base_url(const char *env_value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_AUTHQWEN_4B7464_H */
