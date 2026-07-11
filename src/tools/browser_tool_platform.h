#ifndef SLERMES_BROWSER_TOOL_PLATFORM_H
#define SLERMES_BROWSER_TOOL_PLATFORM_H

#include <stdbool.h>
#include <stdio.h>
#include <json.h>

typedef struct browser_tool_platform browser_tool_platform_t;

browser_tool_platform_t *browser_tool_platform_init(void);
void browser_tool_platform_cleanup(browser_tool_platform_t *s);

bool browser_running_in_docker(void);
bool browser_is_local_mode(void);
char *browser_bare_task_id_for_session_key(const char *session_key);
bool browser_session_info_owned_by_task(const char *session_info_json, const char *task_id, const char *session_key);
int browser_get_session_inactivity_timeout(void);
bool browser_agent_browser_candidate_present(const char *path);

#endif /* SLERMES_BROWSER_TOOL_PLATFORM_H */
