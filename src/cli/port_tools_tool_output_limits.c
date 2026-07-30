/*
 * port_tools_tool_output_limits.c — C port of tools/tool_output_limits.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_tool_output_limits__coerce_positive_int @ tools/tool_output_limits.py:_coerce_positive_int */

/* Port of Python tools/tool_output_limits.py:_coerce_positive_int */
int cli_tools_tool_output_limits__coerce_positive_int(int value, int default_val)
{
    /* Python: returns default if value <= 0 or not convertible.
     * In C we receive an int directly; just check positivity. */
    if (value <= 0) return default_val;
    return value;
}

/* Module-level cache — populated on first call. */
static int g_cached_max_bytes = -1;
static int g_cached_max_lines = -1;
static int g_cached_max_line_length = -1;
static int g_cache_initialized = 0;

/* Note: In the real hermes runtime, load_config() reads config.yaml.
 * For the C port, we provide the defaults. The Python module's defaults
 * match the pre-existing hardcoded values, so behaviour is preserved
 * when the config key is absent. */

/* PoP: cli_tools_tool_output_limits_get_tool_output_limits @ tools/tool_output_limits.py:get_tool_output_limits */

/* Port of Python tools/tool_output_limits.py:get_tool_output_limits */
void cli_tools_tool_output_limits_get_tool_output_limits(int *max_bytes_out, int *max_lines_out, int *max_line_length_out)
{
    if (!g_cache_initialized) {
        g_cached_max_bytes = 50000;      /* DEFAULT_MAX_BYTES */
        g_cached_max_lines = 2000;       /* DEFAULT_MAX_LINES */
        g_cached_max_line_length = 2000; /* DEFAULT_MAX_LINE_LENGTH */
        g_cache_initialized = 1;
    }
    if (max_bytes_out) *max_bytes_out = g_cached_max_bytes;
    if (max_lines_out) *max_lines_out = g_cached_max_lines;
    if (max_line_length_out) *max_line_length_out = g_cached_max_line_length;
}

/* PoP: cli_tools_tool_output_limits__reset_tool_output_limits_cache @ tools/tool_output_limits.py:_reset_tool_output_limits_cache */

/* Port of Python tools/tool_output_limits.py:_reset_tool_output_limits_cache */
void cli_tools_tool_output_limits__reset_tool_output_limits_cache(void)
{
    g_cache_initialized = 0;
    g_cached_max_bytes = -1;
    g_cached_max_lines = -1;
    g_cached_max_line_length = -1;
}

/* PoP: cli_tools_tool_output_limits_get_max_bytes @ tools/tool_output_limits.py:get_max_bytes */

/* Port of Python tools/tool_output_limits.py:get_max_bytes */
int cli_tools_tool_output_limits_get_max_bytes(void)
{
    int mb, ml, mll;
    cli_tools_tool_output_limits_get_tool_output_limits(&mb, &ml, &mll);
    return mb;
}

/* PoP: cli_tools_tool_output_limits_get_max_lines @ tools/tool_output_limits.py:get_max_lines */

/* Port of Python tools/tool_output_limits.py:get_max_lines */
int cli_tools_tool_output_limits_get_max_lines(void)
{
    int mb, ml, mll;
    cli_tools_tool_output_limits_get_tool_output_limits(&mb, &ml, &mll);
    return ml;
}

/* PoP: cli_tools_tool_output_limits_get_max_line_length @ tools/tool_output_limits.py:get_max_line_length */

/* Port of Python tools/tool_output_limits.py:get_max_line_length */
int cli_tools_tool_output_limits_get_max_line_length(void)
{
    int mb, ml, mll;
    cli_tools_tool_output_limits_get_tool_output_limits(&mb, &ml, &mll);
    return mll;
}
