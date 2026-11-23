#ifndef NOC_W11_WEBAPPACCE_9BD4BC_H
#define NOC_W11_WEBAPPACCE_9BD4BC_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w11_webappacce_9bd4bc_sign_in_client @ hermes_cli/webapp_access.py:sign_in_client
 * record: injected _registration(home) result. Returns [client_id, portal] or NULL. */
struct json_t *w11_webappacce_9bd4bc_sign_in_client(const struct json_t *record);
/* PoP: w11_webappacce_9bd4bc_mode @ hermes_cli/webapp_access.py:_mode
 * settings: {enabled, public_url, host}; wildcard is the injected
 * gateway.webapp_listener._is_wildcard(host) seam. Returns a static string
 * ("public"/"lan"/"local") or NULL for None (do not free). */
const char *w11_webappacce_9bd4bc_mode(const struct json_t *settings, int wildcard);
/* PoP: w11_webappacce_9bd4bc_serves_config @ hermes_cli/webapp_access.py:serves_config
 * status: the access_status dict. Returns 1/0. */
int w11_webappacce_9bd4bc_serves_config(const struct json_t *status);
/* PoP: w11_webappacce_9bd4bc_write_config @ hermes_cli/webapp_access.py:_write_config
 * cfg is the injected load_config() result; the returned object is what
 * save_config() would receive. port is a JSON number or null/absent. */
struct json_t *w11_webappacce_9bd4bc_write_config(const struct json_t *cfg, int enabled,
                                                  const char *host,
                                                  const char *public_url,
                                                  const struct json_t *port);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_WEBAPPACCE_9BD4BC_H */
