/* Slermes C port — cron/lifecycle_guard.py (pure gateway-lifecycle guard) */

#define PCRE2_CODE_UNIT_WIDTH 8

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre2.h>

/* Faithful copy of _GATEWAY_LIFECYCLE_PATTERN (lifecycle_guard.py:48).
 * Python's `re` is PCRE-compatible, so PCRE2 reproduces it byte-faithfully
 * (supports \b. Note: POSIX regcomp() does NOT support \b, hence PCRE2). */
static const char *LIFECYCLE_PATTERN =
    "(?i)"
    "(?:hermes\\s+gateway\\s+(?:restart|stop))"
    "|(?:launchctl\\s+(?:kickstart|unload|load|stop|restart)\\b[^\\n]*\\bhermes[.\\-]?gateway)"
    "|(?:systemctl\\s+(?:-\\S+\\s+)*(?:restart|stop|start)\\b[^\\n]*\\bhermes[.\\-]?gateway)"
    "|(?:p?kill\\b[^\\n]*\\bhermes\\b[^\\n]*\\bgateway)"
    "|(?:p?kill\\b[^\\n]*\\bgateway\\b[^\\n]*\\bhermes)";

/* PoP: contains_gateway_lifecycle_command @ cron/lifecycle_guard.py:contains_gateway_lifecycle_command */
bool cron_lifecycle_contains_gateway_lifecycle_command(const char *text)
{
    if (!text || text[0] == '\0') return false;

    int err; PCRE2_SIZE erroff;
    pcre2_code *re = pcre2_compile((PCRE2_SPTR)LIFECYCLE_PATTERN,
                                   PCRE2_ZERO_TERMINATED, 0, &err, &erroff, NULL);
    if (!re) {
        /* Should never happen for a verified pattern; fail closed (no match). */
        return false;
    }
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
    int rc = pcre2_match(re, (PCRE2_SPTR)text, strlen(text), 0, 0, md, NULL);
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    return rc >= 0;
}
