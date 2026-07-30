#ifndef SLERMES_PORT_WEB_TOOLS_H
#define SLERMES_PORT_WEB_TOOLS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;
typedef struct port_web_tools_state port_web_tools_state_t;

/* Lifecycle */
port_web_tools_state_t *port_web_tools_state_init(void);
void port_web_tools_state_cleanup(port_web_tools_state_t *state);

/* Public API */
char *web_env_value(const char *name);
bool web_has_env(const char *name);
char *web_load_web_config(void);
char *web_get_backend(void);
char *web_get_search_backend(void);
char *web_get_extract_backend(void);
char *web_get_capability_backend(const char *capability);
bool web_is_backend_available(const char *backend);
bool web_ddgs_package_importable(void);
char **web_web_requires_env(int *out_count);
int web_get_extract_char_limit(void);
char *web_convert_base64_images_to_links(const char *text);
char *web_store_full_text(const char *url, const char *content);
bool web_truncate_with_footer(const char *content, const char *url, int char_limit, char **out_text, bool *out_truncated);
void web_ensure_web_plugins_loaded(void);
char *web_search_tool(const char *query, int limit);
bool web_check_web_api_key(void);
char *web_extract_tool(const char *urls_json, const char *format, int char_limit);

#endif /* SLERMES_PORT_WEB_TOOLS_H */
