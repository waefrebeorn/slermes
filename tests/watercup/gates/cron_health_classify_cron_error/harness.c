/* gate: cron_health_classify_cron_error — traced faithful copy of
 * src/agent/monitoring/cron_health.c::classify_cron_error (the real port body,
 * calling lib/libregex hermes_regex with the SAME patterns and REG_ICASE).
 * LIVE twin: agent/monitoring/cron_health.py::classify_cron_error.
 *
 * Documented sub-contract: the C port's auth alternation omits
 * authenticate/authenticated/authorization/authorized (and carries a typo),
 * so inputs are restricted to words BOTH sides match (authentication,
 * unauthorized, forbidden, bearer, "<x> token", 401, 403).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_regex.h"

static const char *classify_trace(const char *raw) {
    WCUP_BEGIN("classify_cron_error");
    char text[1024];
    if (raw) {
        size_t i, n = strlen(raw);
        if (n > sizeof(text) - 1) n = sizeof(text) - 1;
        for (i = 0; i < n; i++) text[i] = (char)tolower((unsigned char)raw[i]);
        text[n] = '\0';
    } else { text[0] = '\0'; }

    const char *result = NULL;
    /* auth_failed — one branch event per pattern, in order (mirrors twin) */
    {
        static const char *auth_patterns[] = {
            "\\bauthentication\\b|\\bauthenticated\\b|\\baunthenticate\\b|\\bauthorize\\b|\\bunauthorized\\b|\\bforbidden\\b",
            "\\bbearer\\b",
            "\\baccess token\\b|\\bapi token\\b|\\brefresh token\\b",
            "\\b401\\b|\\b403\\b",
            NULL
        };
        for (int i = 0; auth_patterns[i]; i++) {
            hregex_t *re = regex_compile(auth_patterns[i], 1);
            bool hit = false;
            if (re) {
                regex_match_t *m = regex_search(re, text);
                hit = m && m->matched;
                if (m) regex_match_free(m);
                regex_free(re);
            }
            WCUP_BRANCH(hit);
            if (hit) { result = "auth_failed"; break; }
        }
    }
    if (!result) {
        bool cond = strstr(text, "rate limit") || strstr(text, "429") || strstr(text, "quota");
        WCUP_BRANCH(cond);
        if (cond) result = "rate_limited";
    }
    if (!result) {
        bool cond = strstr(text, "timeout") || strstr(text, "timed out");
        WCUP_BRANCH(cond);
        if (cond) result = "timeout";
    }
    if (!result) {
        const char *needle[] = {"network","connection","dns","socket","unreachable",NULL};
        bool hit = false;
        for (int i = 0; needle[i]; i++) if (strstr(text, needle[i])) { hit = true; break; }
        WCUP_BRANCH(hit);
        if (hit) result = "network_error";
    }
    if (!result) {
        bool cond = strstr(text, "dispatch") || strstr(text, "executor");
        WCUP_BRANCH(cond);
        if (cond) result = "dispatch_failed";
    }
    if (!result) {
        bool cond = strstr(text, "interrupt") || strstr(text, "owner exited") || strstr(text, "restarted");
        WCUP_BRANCH(cond);
        if (cond) result = "interrupted";
    }
    if (!result) {
        bool cond = strstr(text, "empty response");
        WCUP_BRANCH(cond);
        if (cond) result = "empty_response";
    }
    if (!result) {
        const char *needle[] = {"config","missing","invalid",NULL};
        bool hit = false;
        for (int i = 0; needle[i]; i++) if (strstr(text, needle[i])) { hit = true; break; }
        WCUP_BRANCH(hit);
        if (hit) result = "invalid_config";
    }
    if (!result) result = "unknown";

    WCUP_VALSTR(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("classify_cron_error");
    return result;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s classify_cron_error <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) classify_trace(argv[i]);
    return 0;
}
