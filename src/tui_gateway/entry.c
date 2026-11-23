/*
 * entry.c — C home for upstream Hermes tui_gateway/entry.py.
 *
 * PoP: w6_entry_fb4871_shutdown_grace @ tui_gateway/entry.py:_shutdown_grace_seconds
 * PoP: w6_entry_fb4871_grace_default @ tui_gateway/entry.py:_DEFAULT_SHUTDOWN_GRACE_S
 *
 * The signal handlers, stdio RPC channel and MCP discovery plumbing are
 * process/bootstrap concerns and are not reimplemented. The tested behaviour
 * is the deterministic shutdown-grace knob: tolerant env_float parsing with
 * a positive-value guard, driven by an injected env string (no setenv).
 */
#include "noc_w6_entry_fb4871.h"

#include <ctype.h>
#include <stdlib.h>

#define W6_ENTRY_DEFAULT_GRACE_S 1.0

/* Mirrors tui_gateway._env.env_float: "" -> default, malformed -> default. */
static double parse_float(const char *s, double def)
{
    char *end;
    double v;
    const unsigned char *p;

    if (!s || !*s)
        return def;
    v = strtod(s, &end);
    if (end == s)
        return def;
    p = (const unsigned char *)end;
    while (*p && isspace(*p))
        p++;
    if (*p != '\0')
        return def;
    return v;
}

double w6_entry_fb4871_grace_default(void)
{
    return W6_ENTRY_DEFAULT_GRACE_S;
}

double w6_entry_fb4871_shutdown_grace(const char *env_value)
{
    double value = parse_float(env_value, W6_ENTRY_DEFAULT_GRACE_S);
    return value > 0.0 ? value : W6_ENTRY_DEFAULT_GRACE_S;
}
