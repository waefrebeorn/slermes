#ifndef NOC_W15_WEBSERVER_A210CA_H
#define NOC_W15_WEBSERVER_A210CA_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_webserver_a210ca_truncate_token @ hermes_cli/web_server_oauth.py:_truncate_token
 * Returns a JSON string. is_callable models a non-str callable credential. */
struct json_t *w15_webserver_a210ca_truncate_token(const char *value, int visible, int is_callable);

/* PoP: w15_webserver_a210ca_oauth_profile_name @ hermes_cli/web_server_oauth.py:_oauth_profile_name
 * Returns a JSON string or JSON null. */
struct json_t *w15_webserver_a210ca_oauth_profile_name(const char *profile);

/* PoP: w15_webserver_a210ca_token_status @ hermes_cli/web_server_oauth.py:_token_status */
struct json_t *w15_webserver_a210ca_token_status(const char *source, const char *source_label,
                                                const struct json_t *creds, int access_token_callable);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_WEBSERVER_A210CA_H */
