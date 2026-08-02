/*
 * port_tools_tool_output_limits.c — C port of tools/tool_output_limits.py
 */

#include "tool_output.h"

/* PoP: cli_tools_tool_output_limits__coerce_positive_int @ tools/tool_output_limits.py:_coerce_positive_int */
/* Port of Python tools/tool_output_limits.py:_coerce_positive_int */
int cli_tools_tool_output_limits__coerce_positive_int(int value, int default_val)
{
    /* Python: returns default if value <= 0 or not convertible.
     * In C we receive an int directly; just check positivity. */
    if (value <= 0) return default_val;
    return value;
}

/* PoP: cli_tools_tool_output_limits_get_tool_output_limits @ tools/tool_output_limits.py:get_tool_output_limits */
/* Port of Python tools/tool_output_limits.py:get_tool_output_limits */
void cli_tools_tool_output_limits_get_tool_output_limits(int *max_bytes_out, int *max_lines_out, int *max_line_length_out)
{
    /* Delegate to lib/libtooloutput (the canonical port); Python reads the
     * tool_output config section with these defaults, the lib reads the
     * HERMES_TOOL_OUTPUT_* env overrides with the same defaults. */
    if (max_bytes_out) *max_bytes_out = tool_output_get_max_bytes();
    if (max_lines_out) *max_lines_out = tool_output_get_max_lines();
    if (max_line_length_out) *max_line_length_out = tool_output_get_max_line_length();
}

/* PoP: cli_tools_tool_output_limits__reset_tool_output_limits_cache @ tools/tool_output_limits.py:_reset_tool_output_limits_cache */
/* Port of Python tools/tool_output_limits.py:_reset_tool_output_limits_cache */
void cli_tools_tool_output_limits__reset_tool_output_limits_cache(void)
{
    tool_output_reset_cache();
}

/* PoP: cli_tools_tool_output_limits_get_max_bytes @ tools/tool_output_limits.py:get_max_bytes */
/* Port of Python tools/tool_output_limits.py:get_max_bytes */
int cli_tools_tool_output_limits_get_max_bytes(void)
{
    return tool_output_get_max_bytes();
}

/* PoP: cli_tools_tool_output_limits_get_max_lines @ tools/tool_output_limits.py:get_max_lines */
/* Port of Python tools/tool_output_limits.py:get_max_lines */
int cli_tools_tool_output_limits_get_max_lines(void)
{
    return tool_output_get_max_lines();
}

/* PoP: cli_tools_tool_output_limits_get_max_line_length @ tools/tool_output_limits.py:get_max_line_length */
/* Port of Python tools/tool_output_limits.py:get_max_line_length */
int cli_tools_tool_output_limits_get_max_line_length(void)
{
    return tool_output_get_max_line_length();
}
