/*
 * port_compression_facade.c — pure warning messages.
 *
 * Python format rounding: f"{7.5:.0f}" is "8" and f"{2.34:.1f}" is "2.3"
 * (round-half-to-even under the default FE_TONEAREST), so C's %.0f/%.1f output
 * matches directly. ⚠ = U+26A0 = \xe2\x9a\xa0; — = U+2014 = \xe2\x80\x94.
 */

#include "port_compression_facade.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: _warn_commit_overrun @ agent/compression_facade.py:_warn_commit_overrun */
char *cf_warn_commit_overrun(double waited, double ceiling)
{
    char num[64];
    char *out;
    snprintf(num, sizeof(num), "%.0f", waited);
    {/* ceiling rendered into a second slot */
        char cnum[64];
        snprintf(cnum, sizeof(cnum), "%.0f", ceiling);
        size_t n = strlen(
            "\xe2\x9a\xa0 Context compression commit is taking unusually long (")
            + strlen(num) + strlen("s, ceiling ")
            + strlen(cnum)
            + strlen("s). Waiting for it to finish safely \xe2\x80\x94 if this "
                     "persists, check SessionDB health (disk / lock contention).");
        out = malloc(n + 1);
        if (!out) return NULL;
        sprintf(out,
                "\xe2\x9a\xa0 Context compression commit is taking unusually long (%ss, "
                "ceiling %ss). Waiting for it to finish safely \xe2\x80\x94 if this "
                "persists, check SessionDB health (disk / lock contention).",
                num, cnum);
    }
    return out;
}

/* PoP: _report_compression_timeout @ agent/compression_facade.py:_report_compression_timeout */
char *cf_commit_timeout_total(double waited, int progress_observed)
{
    char num[64];
    char *out;
    const char *observed = progress_observed ? " after summary output was observed" : "";
    snprintf(num, sizeof(num), "%.1f", waited);
    {
        size_t n = strlen(
            "\xe2\x9a\xa0 Context compression reached its total ceiling after ")
            + strlen(num) + strlen("s") + strlen(observed)
            + strlen(". No messages were dropped \xe2\x80\x94 continuing without "
                     "compression. Run /compress to retry or /new for a clean session.");
        out = malloc(n + 1);
        if (!out) return NULL;
        sprintf(out,
                "\xe2\x9a\xa0 Context compression reached its total ceiling after %ss%s"
                ". No messages were dropped \xe2\x80\x94 continuing without compression. "
                "Run /compress to retry or /new for a clean session.", num, observed);
    }
    return out;
}

/* PoP: _report_compression_timeout @ agent/compression_facade.py:_report_compression_timeout */
char *cf_commit_timeout_stalled(double idle)
{
    char num[64];
    char *out;
    snprintf(num, sizeof(num), "%.1f", idle);
    {
        size_t n = strlen(
            "\xe2\x9a\xa0 Context compression timed out after ") + strlen(num)
            + strlen("s with no output from the summary model. No messages were "
                     "dropped \xe2\x80\x94 continuing without compression. Run "
                     "/compress to retry, /new for a clean session, or check "
                     "auxiliary.compression.");
        out = malloc(n + 1);
        if (!out) return NULL;
        sprintf(out,
                "\xe2\x9a\xa0 Context compression timed out after %ss with no output "
                "from the summary model. No messages were dropped \xe2\x80\x94 "
                "continuing without compression. Run /compress to retry, /new for "
                "a clean session, or check auxiliary.compression.", num);
    }
    return out;
}
