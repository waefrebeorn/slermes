#ifndef AGENT_PORT_DISPLAY_TOOL_PREVIEW_H
#define AGENT_PORT_DISPLAY_TOOL_PREVIEW_H

#include <stddef.h>
#include "hermes_json.h"

/* PoP ports from agent/display.py — see port_display_tool_preview.c.
 * build_* functions take tool_name + args (JSON object string) + max_len
 * (0 = unlimited) and return a malloc'd string (caller frees).
 * build_tool_label's friendly flag: 0 = raw preview, non-zero = friendly verb. */

char *cli_agent_display__truncate_preview(const char *text, int max_len);
char *cli_agent_display__read_file_line_label(const json_t *args);
char *cli_agent_display__redact_tool_args_for_display(const char *tool_name, const char *args_json);
char *cli_agent_display__build_tool_preview(const char *tool_name, const char *args_json, int max_len);
char *cli_agent_display__build_tool_label(const char *tool_name, const char *args_json, int max_len, int friendly);

#endif
