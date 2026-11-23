#ifndef NOC_W8_CLIENTLIFEC_20BD72_H
#define NOC_W8_CLIENTLIFEC_20BD72_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_clientlifec_20bd72_valid_credential_pair @ agent/client_lifecycle.py:_valid_credential_pair */
int w8_clientlifec_20bd72_valid_credential_pair(const struct json_t *api_key,
                                                const struct json_t *base_url);

/* PoP: w8_clientlifec_20bd72_reset_slot @ agent/client_lifecycle.py:_reset_slot
 * Returns the resulting cache object (all original keys preserved). */
struct json_t *w8_clientlifec_20bd72_reset_slot(const struct json_t *cache, int in_use);

/* PoP: w8_clientlifec_20bd72_api_kwargs_have_image_parts @ agent/client_lifecycle.py:ClientLifecycleMixin._api_kwargs_have_image_parts */
int w8_clientlifec_20bd72_api_kwargs_have_image_parts(const struct json_t *api_kwargs);

/* PoP: w8_clientlifec_20bd72_is_openai_client_closed @ agent/client_lifecycle.py:ClientLifecycleMixin._is_openai_client_closed
 * client representation: {"mock":bool, "is_closed":bool|{"$call":bool},
 * "_client":{"is_closed":bool}} — mirrors the getattr/callable logic. */
int w8_clientlifec_20bd72_is_openai_client_closed(const struct json_t *client);

/* PoP: w8_clientlifec_20bd72_should_adopt_env_credentials @ agent/client_lifecycle.py:ClientLifecycleMixin._should_adopt_env_credentials
 * args: api_key, base_url, default_base, state{base_url,api_key,_env_creds_seen,
 * _credential_pool,_credential_pool_entry_id}. */
int w8_clientlifec_20bd72_should_adopt_env_credentials(const struct json_t *args);

/* PoP: w8_clientlifec_20bd72_anthropic_log_context @ agent/client_lifecycle.py:ClientLifecycleMixin._anthropic_log_context */
struct json_t *w8_clientlifec_20bd72_anthropic_log_context(const struct json_t *agent);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_CLIENTLIFEC_20BD72_H */
