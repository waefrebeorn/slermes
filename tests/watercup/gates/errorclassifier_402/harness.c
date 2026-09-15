/* gate: errorclassifier_402 — traced copy of
 * lib/liberrorclassifier/error_classifier.c::classify_402
 * (LIVE twin: agent/error_classifier.py::_classify_402).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: usage-limit pattern present
 *   B2: transient signal present
 *   outcome = (B1 && B2) ? "rate_limit" : "billing"
 *   VSTR(outcome)
 *
 * Inputs are pre-lowercased error messages (LIVE lowercases upstream).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static const char *USAGE_LIMIT_PATTERNS[] = {
    "usage limit", "quota", "limit exceeded", "key limit exceeded",
};

static const char *USAGE_LIMIT_TRANSIENT[] = {
    "try again", "retry", "resets at", "reset in", "wait",
    "requests remaining", "periodic", "window",
};

static int contains_any_i(const char *haystack, const char **pats, int n)
{
    if (!haystack) return 0;
    char low[4096];
    size_t i = 0;
    for (; haystack[i] && i + 1 < sizeof low; i++)
        low[i] = (char)tolower((unsigned char)haystack[i]);
    low[i] = '\0';
    for (int j = 0; j < n; j++)
        if (strstr(low, pats[j])) return 1;
    return 0;
}

static void wc_classify_402(const char *error_msg)
{
    WCUP_BEGIN("errorclassifier_402");

    int has_usage = contains_any_i(error_msg, USAGE_LIMIT_PATTERNS, 4);
    WCUP_BRANCH(has_usage);                       /* B1 */
    int has_transient = contains_any_i(error_msg, USAGE_LIMIT_TRANSIENT, 8);
    WCUP_BRANCH(has_transient);                   /* B2 */

    const char *outcome = (has_usage && has_transient) ? "rate_limit" : "billing";
    WCUP_VALSTR(outcome);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("errorclassifier_402");
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s fn <msg>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        wc_classify_402(argv[i]);
    return 0;
}
