/*
 * port_banner.h -- pure display formatting ported from hermes_cli/banner.py.
 *
 * See port_banner.c: only the dependency-free string formatting is ported; the
 * update checks, skin lookups and rich rendering stay in Python.
 */
#ifndef PORT_BANNER_H
#define PORT_BANNER_H

#include <stddef.h>

/* banner.py:_plural -- "commit" / "commits". Caller frees. */
char *banner_plural(int n, const char *word);

/* banner.py:_format_context_length -- 128000 -> "128K", 1048576 -> "1M".
 * Decimal units; one decimal place when the value is not within 0.05 of a
 * whole unit. Caller frees. */
char *banner_format_ctx_length(long long tokens);

/* banner.py:_display_toolset_name -- strips a "_tools" suffix; empty input
 * yields "unknown" (Python truthiness, not NULL-check). Caller frees. */
char *banner_toolset_name(const char *toolset_name);

#endif /* PORT_BANNER_H */
