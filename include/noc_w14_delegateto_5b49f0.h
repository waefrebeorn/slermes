#ifndef NOC_W14_DELEGATETO_5B49F0_H
#define NOC_W14_DELEGATETO_5B49F0_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_delegateto_5b49f0_parse_timeout @ tools/delegate_tool_config.py:_parse_timeout */
struct json_t *w14_delegateto_5b49f0_parse_timeout(const struct json_t *raw);
/* PoP: w14_delegateto_5b49f0_normalized_runtime_url @ tools/delegate_tool_config.py:_normalized_runtime_url */
struct json_t *w14_delegateto_5b49f0_normalized_runtime_url(const struct json_t *value);
/* PoP: w14_delegateto_5b49f0_merge_request_overrides @ tools/delegate_tool_config.py:_merge_request_overrides */
struct json_t *w14_delegateto_5b49f0_merge_request_overrides(const struct json_t *runtime_overrides,
                                           const struct json_t *explicit_overrides);
/* PoP: w14_delegateto_5b49f0_credential_bundle @ tools/delegate_tool_config.py:_credential_bundle */
struct json_t *w14_delegateto_5b49f0_credential_bundle(const struct json_t *model, const struct json_t *provider,
                                     const struct json_t *base_url, const struct json_t *api_key,
                                     const struct json_t *api_mode, const struct json_t *request_overrides,
                                     const struct json_t *extra);
/* PoP: w14_delegateto_5b49f0_inherit_parent_capabilities @ tools/delegate_tool_config.py:_inherit_parent_capabilities */
struct json_t *w14_delegateto_5b49f0_inherit_parent_capabilities(const struct json_t *parent_agent,
                                               const struct json_t *override_provider,
                                               const struct json_t *override_base_url);
/* PoP: w14_delegateto_5b49f0_gateway_delegation_inline @ tools/delegate_tool_config.py:gateway_delegation_inline */
int w14_delegateto_5b49f0_gateway_delegation_inline(const struct json_t *user_config, const char *platform_key,
                                  int interim_enabled);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_DELEGATETO_5B49F0_H */
