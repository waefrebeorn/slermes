#ifndef NOC_W16_WEBSERVER_33D5D4_H
#define NOC_W16_WEBSERVER_33D5D4_H

#include <stdbool.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_webserver_33d5d4_infer_type @ hermes_cli/web_server_config.py:_infer_type */
struct json_t *w16_webserver_33d5d4_infer_type(const struct json_t *value);

/* PoP: w16_webserver_33d5d4_is_command_provider_block @ hermes_cli/web_server_config.py:_is_command_provider_block */
bool w16_webserver_33d5d4_is_command_provider_block(const struct json_t *value);

/* PoP: w16_webserver_33d5d4_normalize_config_for_web @ hermes_cli/web_server_config.py:_normalize_config_for_web */
struct json_t *w16_webserver_33d5d4_normalize_config_for_web(const struct json_t *config);

/* PoP: w16_webserver_33d5d4_endpoint_known_to_config @ hermes_cli/web_server_config.py:_endpoint_known_to_config */
bool w16_webserver_33d5d4_endpoint_known_to_config(const struct json_t *cfg,
                                                   const char *base_url);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_WEBSERVER_33D5D4_H */
