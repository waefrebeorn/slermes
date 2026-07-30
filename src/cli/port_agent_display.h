#ifndef CLI_PORT_AGENT_DISPLAY_H
#define CLI_PORT_AGENT_DISPLAY_H

#include <stddef.h>

/* PoP ports from agent/display.py — see port_agent_display.c.
 * Caller frees returned strings / string arrays. */

char **cli_agent_display__split_shell_words(const char *segment, int *count);
char *cli_agent_display__strip_shell_pipe_tail(const char *segment);
char **cli_agent_display__split_shell_compound(const char *command, int *count);
char *cli_agent_display__clean_shell_segment(const char *segment);
int cli_agent_display__is_shell_boundary_echo(const char *segment);
char *cli_agent_display__summarize_shell_command(const char *command);
char *cli_agent_display__oneline(const char *text);

/* Tool preview/label + friendly-label state (see port_display_tool_preview.c) */
struct json_t;
char *cli_agent_display__truncate_preview(const char *text, int max_len);
char *cli_agent_display__read_file_line_label(const struct json_t *args);
char *cli_agent_display__redact_tool_args_for_display(const char *tool_name, const char *args_json);
char *cli_agent_display__build_tool_preview(const char *tool_name, const char *args_json, int max_len);
char *cli_agent_display__build_tool_label(const char *tool_name, const char *args_json, int max_len, int friendly);
char *cli_agent_display__redact_browser_typed_text_for_display(const char *value_json, const char *typed_text);
void  cli_agent_display__set_friendly_tool_labels(int enabled);
int   cli_agent_display__get_friendly_tool_labels(void);
const char *cli_agent_display__get_tool_verb(const char *tool_name);
const char *cli_agent_display__tool_verb_connector(const char *tool_name);
int   cli_agent_display__verb_drops_preview(const char *tool_name);

#endif
