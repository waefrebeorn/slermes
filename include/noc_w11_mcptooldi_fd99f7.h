#ifndef NOC_W11_MCPTOOLDI_FD99F7_H
#define NOC_W11_MCPTOOLDI_FD99F7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_mcptooldi_fd99f7_connected_summary @ tools/mcp_tool_discovery.py:_connected_summary
 * servers: name -> {"tools":<int>,"park":<bool>}; connect_errors: name -> reason.
 * Returns {"tool_count":int,"connected":int,"failed":[[name,reason],...]}. */
struct json_t *w11_mcptooldi_fd99f7_connected_summary(const struct json_t *names,
        const struct json_t *servers, const struct json_t *connect_errors,
        long long lazy_tools, long long lazy_servers);

/* PoP: w11_mcptooldi_fd99f7_unregistered @ tools/mcp_tool_discovery.py:_unregistered */
struct json_t *w11_mcptooldi_fd99f7_unregistered(const struct json_t *wanted,
        const struct json_t *servers, const struct json_t *connecting,
        const struct json_t *lazy_configs);

/* PoP: w11_mcptooldi_fd99f7_awaiting_connect @ tools/mcp_tool_discovery.py:_awaiting_connect */
struct json_t *w11_mcptooldi_fd99f7_awaiting_connect(const struct json_t *wanted,
        const struct json_t *servers, const struct json_t *connecting,
        const struct json_t *lazy_configs, const struct json_t *retry_after, double now);

/* PoP: w11_mcptooldi_fd99f7_not_connected @ tools/mcp_tool_discovery.py:_not_connected */
struct json_t *w11_mcptooldi_fd99f7_not_connected(const struct json_t *wanted,
        const struct json_t *servers, const struct json_t *connect_errors,
        const struct json_t *connecting, const struct json_t *lazy_configs,
        const struct json_t *retry_after, double now);

/* PoP: w11_mcptooldi_fd99f7_parallel_safe @ tools/mcp_tool_discovery.py:is_mcp_tool_parallel_safe */
int w11_mcptooldi_fd99f7_parallel_safe(const char *tool_name,
        const struct json_t *mcp_tool_server_names, const struct json_t *parallel_safe);

/* PoP: w11_mcptooldi_fd99f7_resolve_server_lazy @ tools/mcp_tool_discovery.py:_resolve_server_lazy */
int w11_mcptooldi_fd99f7_resolve_server_lazy(const struct json_t *config);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_MCPTOOLDI_FD99F7_H */
