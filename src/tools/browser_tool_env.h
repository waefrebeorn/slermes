#ifndef SLERMES_BROWSER_TOOL_ENV_H
#define SLERMES_BROWSER_TOOL_ENV_H

#include <stdbool.h>
#include <stdio.h>
#include <json.h>

typedef struct browser_tool_env browser_tool_env_t;

browser_tool_env_t *browser_tool_env_init(void);
void browser_tool_env_cleanup(browser_tool_env_t *s);

char *browser_build_browser_env(void);
char *browser_sanitize_url_for_logs(const char *value);
int browser_get_command_timeout(void);
int browser_safe_command_timeout(void);
int browser_get_open_command_timeout(bool first_open);
bool browser_needs_chromium_sandbox_bypass(void);
void browser_read_command_output_files(const char *stdout_path, const char *stderr_path, char **out_stdout, char **out_stderr);
void browser_unlink_command_output_files(int count, const char **paths);
char *browser_format_timeout_error(const char *command, int timeout, const char *stdout_text, const char *stderr_text);

#endif /* SLERMES_BROWSER_TOOL_ENV_H */
