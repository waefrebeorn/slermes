#ifndef NOC_W12_WEBSERVER_3794AF_H
#define NOC_W12_WEBSERVER_3794AF_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_webserver_3794af_normalize_mcp_server_create @ hermes_cli/web_server_mcp.py:normalize_mcp_server_create
 * Input object carries name/url/command/auth/bearer_token/args/env.  Returns
 * {"name":..,"config":..,"token":..} on success or {"error":<ValueError msg>}.
 * Caller frees. */
struct json_t *w12_webserver_3794af_normalize_mcp_server_create(const struct json_t *body);

/* PoP: w12_webserver_3794af_redact_mcp_env @ hermes_cli/web_server_mcp.py:_redact_mcp_env
 * dict[str, Any] -> dict[str, str] with secret-shaped values masked; caller frees. */
struct json_t *w12_webserver_3794af_redact_mcp_env(const struct json_t *env);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_WEBSERVER_3794AF_H */
