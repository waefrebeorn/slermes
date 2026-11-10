/*
 * port_banner.c
 *
 * Pure helpers ported from hermes_cli/banner.py - the startup banner's display
 * formatting.
 *
 * That module also owns update checks, skin lookups, rich-markup rendering and
 * install-stamp reads; those stay in Python because they are I/O or need the
 * rich library. Only the dependency-free string formatting is here:
 *
 *   banner_plural            <- banner.py:_plural
 *   banner_format_ctx_length <- banner.py:_format_context_length
 *   banner_toolset_name      <- banner.py:_display_toolset_name
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* ===========================================================================
 *  _plural  (banner.py:_plural)
 *
 *      word if n == 1 else f"{word}s"
 *
 *  English pluralisation only, exactly as the live code does it - no -es, no
 *  irregulars. Returns a NEW string the caller frees.
 * =========================================================================== */

/* PoP: _plural @ hermes_cli/banner.py:_plural */
char *banner_plural(int n, const char *word)
{
    const char *w = word ? word : "";
    size_t len = strlen(w);
    char *out = (char *)malloc(len + 2);
    if (!out) return NULL;
    memcpy(out, w, len);
    if (n != 1) out[len++] = 's';
    out[len] = '\0';
    return out;
}

/* ===========================================================================
 *  _format_context_length  (banner.py:_format_context_length)
 *
 *      for unit, div in (("M", 1_000_000), ("K", 1_000)):
 *          if tokens >= div:
 *              val = tokens / div
 *              rounded = round(val)
 *              return f"{rounded}{unit}" if abs(val - rounded) < 0.05 else f"{val:.1f}{unit}"
 *      return str(tokens)
 *
 *  Decimal (1000-based) units, not binary. The 0.05 threshold decides between
 *  the integer and one-decimal rendering, so 128000 -> "128K" but
 *  123456 -> "123.5K". Returns a NEW string the caller frees.
 * =========================================================================== */

/* PoP: _format_context_length @ hermes_cli/banner.py:_format_context_length */
char *banner_format_ctx_length(long long tokens)
{
    static const struct { const char *unit; long long div; } UNITS[] = {
        { "M", 1000000LL },
        { "K", 1000LL },
    };
    char buf[64];
    for (size_t i = 0; i < sizeof(UNITS) / sizeof(UNITS[0]); i++) {
        if (tokens >= UNITS[i].div) {
            double val = (double)tokens / (double)UNITS[i].div;
            double rounded = round(val);
            if (fabs(val - rounded) < 0.05)
                snprintf(buf, sizeof buf, "%.0f%s", rounded, UNITS[i].unit);
            else
                snprintf(buf, sizeof buf, "%.1f%s", val, UNITS[i].unit);
            char *out = (char *)malloc(strlen(buf) + 1);
            if (!out) return NULL;
            strcpy(out, buf);
            return out;
        }
    }
    snprintf(buf, sizeof buf, "%lld", tokens);
    char *out = (char *)malloc(strlen(buf) + 1);
    if (!out) return NULL;
    strcpy(out, buf);
    return out;
}

/* ===========================================================================
 *  _display_toolset_name  (banner.py:_display_toolset_name)
 *
 *      toolset_name.removesuffix("_tools") if toolset_name else "unknown"
 *
 *  Empty (not NULL) also falls through to "unknown" because the Python test is
 *  truthiness, not `is not None`. Returns a NEW string the caller frees.
 * =========================================================================== */

/* PoP: _display_toolset_name @ hermes_cli/banner.py:_display_toolset_name */
char *banner_toolset_name(const char *toolset_name)
{
    const char *suffix = "_tools";
    const char *in = (toolset_name && toolset_name[0]) ? toolset_name : "unknown";

    size_t len = strlen(in);
    size_t slen = strlen(suffix);
    if (len >= slen && strcmp(in + len - slen, suffix) == 0) len -= slen;

    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, in, len);
    out[len] = '\0';
    return out;
}
