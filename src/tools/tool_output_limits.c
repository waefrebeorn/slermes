/*
 * tool_output_limits.c — Name parity wrapper for Python tools/tool_output_limits.py
 *
 * NOTE: The C implementation lives in lib/libtooloutput/tool_output.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python tools/tool_output_limits.py.
 * C implementation: lib/libtooloutput/tool_output.c
 *
 * Configurable tool-output truncation limits
 *
 * Key functions ported:
 *   tool_output_limit_get, tool_output_truncate
 */
#include "hermes_core_types.h"

/* === merged from port_tools_tool_output_limits.c === */
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


/* PoP: tools_tool_output_limits_u_coerce_positive_int
 * @ tools/tool_output_limits.py:_coerce_positive_int */
/* _coerce_int(value, default, 1): int(value) (python int() on a STRING -- strict,
 * no decimal/exponent) or default; then default if iv < 1. Input "value\tdefault". */
char *tools_tool_output_limits_u_coerce_positive_int(const char *arg) {
    const char *a = arg ? arg : "";
    const char *tab = strchr(a, '\t');
    char val[64]; size_t vlen = tab ? (size_t)(tab - a) : strlen(a);
    if (vlen >= sizeof val) vlen = sizeof val - 1;
    memcpy(val, a, vlen); val[vlen] = 0;
    long def = (tab && *(tab+1)) ? strtol(tab + 1, NULL, 10) : 0;
    char *p = val;
    while (*p==' '||*p=='\t'||*p=='\n'||*p=='\r'||*p=='\v'||*p=='\f') p++;
    char *end = p; int ok = 0, neg = 0;
    if (*end=='+'||*end=='-') { neg = (*end=='-'); end++; }
    const char *digits = end;
    while (*end>='0' && *end<='9') { end++; ok = 1; }
    if (ok) { char *q = end; while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r'||*q=='\v'||*q=='\f') q++; if (*q) ok = 0; }
    long iv = 0;
    if (ok && digits != end) { char tmp[64]; size_t n = (size_t)(end - p); if (n >= sizeof tmp) n = sizeof tmp - 1; memcpy(tmp, p, n); tmp[n]=0; iv = strtol(tmp, NULL, 10); }
    long res = (!ok || iv < 1) ? def : iv;
    char buf[64]; snprintf(buf, sizeof buf, "%ld", res); return strdup(buf);
}
