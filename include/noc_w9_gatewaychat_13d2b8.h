#ifndef NOC_W9_GATEWAYCHAT_13D2B8_H
#define NOC_W9_GATEWAYCHAT_13D2B8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_gatewaychat_13d2b8_continue_title @ hermes_cli/gateway_chat.py:continue_title */
struct json_t *w9_gatewaychat_13d2b8_continue_title(const struct json_t *args);
/* PoP: w9_gatewaychat_13d2b8_wants_latest @ hermes_cli/gateway_chat.py:wants_latest */
struct json_t *w9_gatewaychat_13d2b8_wants_latest(const struct json_t *args);
/* PoP: w9_gatewaychat_13d2b8_env_launch_flags @ hermes_cli/gateway_chat.py:env_launch_flags
 * env is an injected mapping instead of os.environ. */
struct json_t *w9_gatewaychat_13d2b8_env_launch_flags(const struct json_t *env);
/* PoP: w9_gatewaychat_13d2b8_bypass_launch @ hermes_cli/gateway_chat.py:bypass_launch */
struct json_t *w9_gatewaychat_13d2b8_bypass_launch(const struct json_t *args, const struct json_t *env);
/* PoP: w9_gatewaychat_13d2b8_launch_flags @ hermes_cli/gateway_chat.py:_launch_flags */
struct json_t *w9_gatewaychat_13d2b8_launch_flags(const struct json_t *args);
/* PoP: w9_gatewaychat_13d2b8_requested_policy @ hermes_cli/gateway_chat.py:_requested_policy */
struct json_t *w9_gatewaychat_13d2b8_requested_policy(const struct json_t *args, const struct json_t *env);
/* PoP: w9_gatewaychat_13d2b8_validate_options @ hermes_cli/gateway_chat.py:validate_options
 * Returns the GatewayClientError message as a string, or null when valid. */
struct json_t *w9_gatewaychat_13d2b8_validate_options(const struct json_t *args, const struct json_t *env);
/* PoP: w9_gatewaychat_13d2b8_resume_policy_error @ hermes_cli/gateway_chat.py:resume_policy_error
 * Returns the GatewayClientError message, or null for an empty key list. */
struct json_t *w9_gatewaychat_13d2b8_resume_policy_error(const struct json_t *keys);
/* PoP: w9_gatewaychat_13d2b8_client_env_policy @ hermes_cli/gateway_chat.py:client_env_policy
 * client_env is the injected shell_client_env() result. */
struct json_t *w9_gatewaychat_13d2b8_client_env_policy(const struct json_t *parameters,
                                                       const struct json_t *client_env);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_GATEWAYCHAT_13D2B8_H */
