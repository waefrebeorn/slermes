#ifndef GATEWAY_PORT_DISPLAY_CONFIG_H
#define GATEWAY_PORT_DISPLAY_CONFIG_H

#include "hermes_json.h"

/* Faithful ports of gateway/display_config.py (pure transforms, oracle-verified).
 * Return OWNED json_t nodes; caller frees with json_free(). */
json_t *display_config__normalise_node(const char *setting, const json_t *value);
json_t *display_config__resolve_node(const json_t *user_config,
                                     const char *platform_key,
                                     const char *setting,
                                     const json_t *fallback);

/* Convenience wrappers: return freshly malloc'd canonical JSON strings
 * (json.dumps(ensure_ascii=False) form). Caller frees with free(). */
char *display_config__normalise(const char *setting, const json_t *value);
char *display_config__resolve(const json_t *user_config, const char *platform_key,
                              const char *setting, const json_t *fallback);

#endif /* GATEWAY_PORT_DISPLAY_CONFIG_H */
