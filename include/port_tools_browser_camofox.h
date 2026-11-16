/*
 * port_tools_browser_camofox.h — Public surface for port_tools_browser_camofox.c
 */
#ifndef PORT_TOOLS_BROWSER_CAMOFOX_H
#define PORT_TOOLS_BROWSER_CAMOFOX_H

#include <stdbool.h>

typedef struct json_t json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Camofox URL resolution */
char *get_camofox_url(void);
char *config_cdp_url(const char *camofox_url);
bool check_camofox_available(void);
char *get_vnc_url(void);

/* Camofox config helpers */
json_t *get_camofox_config(void);
bool managed_persistence_enabled(void);
char *camofox_identity_override(void);
bool env_flag(const char *flag_name, bool default_val);
bool adopt_existing_tab_enabled(void);
bool loopback_rewrite_enabled(void);
char *loopback_rewrite_host(void);
bool is_loopback_hostname(const char *hostname);
char *rewrite_loopback_url_for_camofox(const char *url);

/* Tab management */
char *_adopt_existing_tab(const char *camofox_url, const char *url);
char *ensure_tab(const char *camofox_url, const char *url, bool new_window);
void drop_session(const char *camofox_url, const char *session_id);
void camofox_soft_cleanup(const char *camofox_url);

/* Low-level HTTP helpers */
char *get_raw(const char *camofox_url, const char *path);

/* Navigation */
char *camofox_navigate(const char *camofox_url, const char *session_id, const char *url);

/* Input simulation */
char *camofox_click(const char *camofox_url, const char *session_id, int x, int y, bool right_click);
char *camofox_type(const char *camofox_url, const char *session_id, const char *text);
char *camofox_scroll(const char *camofox_url, const char *session_id, int dx, int dy);

/* Navigation controls */
char *camofox_back(const char *camofox_url, const char *session_id);
char *camofox_press(const char *camofox_url, const char *session_id, const char *key);

/* Session control */
char *camofox_close(const char *camofox_url, const char *session_id);

/* Snapshot & media */
char *camofox_snapshot(const char *camofox_url, const char *session_id);
json_t *camofox_get_images(const char *camofox_url, const char *session_id);

/* Vision */
json_t *camofox_vision(const char *camofox_url, const char *session_id, const char *prompt);

/* Console access */
char *camofox_console(const char *camofox_url, const char *session_id, const char *cmd);

/* Private page block helper */
bool camofox_private_page_block(const char *url);

#ifdef __cplusplus
}
#endif

#endif /* PORT_TOOLS_BROWSER_CAMOFOX_H */