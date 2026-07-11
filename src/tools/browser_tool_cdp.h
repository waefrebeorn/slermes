#ifndef SLERMES_BROWSER_TOOL_CDP_H
#define SLERMES_BROWSER_TOOL_CDP_H

#include <stdbool.h>
#include <stdio.h>
#include <json.h>

typedef struct browser_tool_cdp browser_tool_cdp_t;

browser_tool_cdp_t *browser_tool_cdp_init(void);
void browser_tool_cdp_cleanup(browser_tool_cdp_t *s);

char *browser_get_vision_model(void);
char *browser_get_extraction_model(void);
char *browser_resolve_cdp_override(const char *cdp_url);
char *browser_get_cdp_override(void);
char *browser_get_dialog_policy_config(void);
json_t *browser_ensure_cdp_supervisor(const char *task_id);
json_t *browser_stop_cdp_supervisor(const char *task_id);
bool browser_using_lightpanda_engine(void);
void browser_copy_fallback_warning(char *dest, size_t dest_size);
bool browser_auto_local_for_private_urls(void);
bool browser_url_is_private(const char *url);
char *browser_navigation_session_key(const char *task_id);
char *browser_socket_safe_tmpdir(void);
json_t *browser_create_local_session(const char *task_id);
json_t *browser_create_cdp_session(const char *task_id, const char *cdp_url);
json_t *browser_get_session_info(const char *session_key);
char *browser_find_agent_browser(void);
char *browser_truncate_snapshot(const char *snapshot, size_t max_chars);
json_t *browser_check_browser_requirements(void);
json_t *browser_check_browser_vision_requirements(void);

#endif /* SLERMES_BROWSER_TOOL_CDP_H */
