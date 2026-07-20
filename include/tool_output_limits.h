/*
 * tool_output_limits.h — minimal declaration surface for the deterministic
 * helpers ported from tools/tool_output_limits.py in
 * src/cli/port_tools_tool_output_limits.c. Opaque / minimal: no god-header.
 */

#ifndef SLERMES_TOOL_OUTPUT_LIMITS_H
#define SLERMES_TOOL_OUTPUT_LIMITS_H

/* Port of tools/tool_output_limits.py:_coerce_positive_int (value already an int). */
int cli_tools_tool_output_limits__coerce_positive_int(int value, int default_val);

/* Port of tools/tool_output_limits.py:get_max_bytes / get_max_lines /
 * get_max_line_length. With no config they return the module defaults. */
int cli_tools_tool_output_limits_get_max_bytes(void);
int cli_tools_tool_output_limits_get_max_lines(void);
int cli_tools_tool_output_limits_get_max_line_length(void);

#endif /* SLERMES_TOOL_OUTPUT_LIMITS_H */
