/*
 * compression_facade.c — C home for upstream Hermes agent/compression_facade.py.
 *
 * PoP: w10_compression_e64919_timeout_fallback_prompt @ agent/compression_facade.py:_timeout_fallback_prompt
 * PoP: w10_compression_e64919_warn_commit_overrun @ agent/compression_facade.py:_warn_commit_overrun
 * PoP: w10_compression_e64919_compression_timeout_warning @ agent/compression_facade.py:_report_compression_timeout
 *
 * The host-side _compress_context wrapper is thread/fence/agent-MRO bound and
 * stays in Python. These exports cover its pure decision and warning-text core:
 * the lazy prompt fallback and the two user-visible timeout messages.
 */
#include "noc_w10_compression_e64919.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

char *w10_compression_e64919_timeout_fallback_prompt(const char *cached, int build_ok,
                                                     const char *built, const char *system_message) {
    if (cached && cached[0]) return xstrdup(cached);
    if (build_ok) return xstrdup(built);
    return xstrdup(system_message);
}

char *w10_compression_e64919_warn_commit_overrun(double waited, double ceiling) {
    char buf[512];
    snprintf(buf, sizeof(buf),
             "⚠ Context compression commit is taking unusually long (%.0fs, ceiling %.0fs). "
             "Waiting for it to finish safely — if this persists, check SessionDB health (disk / lock contention).",
             waited, ceiling);
    return xstrdup(buf);
}

char *w10_compression_e64919_compression_timeout_warning(double idle, double waited,
                                                         double total_ceiling, double since_progress,
                                                         int total_exhausted, int progress_observed) {
    (void)total_ceiling;
    (void)since_progress;
    char buf[768];
    if (total_exhausted) {
        const char *progress = progress_observed ? " after summary output was observed" : "";
        snprintf(buf, sizeof(buf),
                 "⚠ Context compression reached its total ceiling after %.1fs%s. No messages were "
                 "dropped — continuing without compression. Run /compress to retry or /new for a clean session.",
                 waited, progress);
    } else {
        snprintf(buf, sizeof(buf),
                 "⚠ Context compression timed out after %.1fs with no output from the summary model. "
                 "No messages were dropped — continuing without compression. Run /compress to retry, /new "
                 "for a clean session, or check auxiliary.compression.",
                 idle);
    }
    return xstrdup(buf);
}

