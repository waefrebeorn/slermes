/* gate: context_compressor_is_summary_access_or_quota_error — traced copy of
 * src/agent/context_compressor_pure.c::cc_is_summary_access_or_quota_error
 * (LIVE twin: agent/context_compressor.py::_is_summary_access_or_quota_error).
 *
 * Input encoding: "<reason>|<status>|<text>"
 *   reason: 0 unknown, 35 auth, 36 auth_permanent, 40 rate_limit,
 *           2 billing (mirrors the port's failover-reason codes)
 *   status: HTTP status code or 0 for none.
 *
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: reason == FAILOVER_RATE_LIMIT      -> false
 *   B2: reason in {AUTH, AUTH_PERMANENT}   -> true
 *   V1: lowercased error text
 *   B3: any missing-credential marker hit  -> true
 *   B4: status in {401, 402, 403}          -> true
 *   B5: any permanent-quota marker hit     -> true
 *   V2: result "True"/"False"
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

/* verbatim from src/agent/context_compressor_pure.c */
static const char *SUMMARY_MISSING_CREDENTIAL_MARKERS[] = {
    "no api key was found",
    "no api key found",
    NULL,
};
static const char *SUMMARY_PERMANENT_QUOTA_MARKERS[] = {
    "insufficient_quota",
    "quota exceeded",
    "quota_exceeded",
    "out of funds",
    "out of credits",
    "out of credit",
    "out of extra usage",
    NULL,
};

static void wc_cc_is_quota_error(int classified_reason, int status_code,
                                 const char *error_text) {
    WCUP_BEGIN("is_summary_access_or_quota_error");
    bool result = false;
    if (classified_reason == 40 /* FAILOVER_RATE_LIMIT */) {   /* B1 */
        WCUP_BRANCH(1);
        goto done;
    }
    WCUP_BRANCH(0);
    if (classified_reason == 35 /* AUTH */ ||
        classified_reason == 36 /* AUTH_PERMANENT */) {         /* B2 */
        WCUP_BRANCH(1);
        result = true;
        goto done;
    }
    WCUP_BRANCH(0);
    {
        const char *err = error_text ? error_text : "";
        size_t elen = strlen(err);
        char *low = malloc(elen + 1);
        for (size_t i = 0; i < elen; i++) low[i] = (char)tolower((unsigned char)err[i]);
        low[elen] = '\0';
        WCUP_VALSTR(low);                                       /* V1 */
        bool hit = false;
        for (int i = 0; SUMMARY_MISSING_CREDENTIAL_MARKERS[i]; i++)
            if (strstr(low, SUMMARY_MISSING_CREDENTIAL_MARKERS[i])) { hit = true; break; }
        WCUP_BRANCH(hit);                                       /* B3 */
        if (!hit) {
            int st = (status_code == 401 || status_code == 402 || status_code == 403);
            WCUP_BRANCH(st);                                    /* B4 */
            if (!st) {
                for (int i = 0; SUMMARY_PERMANENT_QUOTA_MARKERS[i]; i++)
                    if (strstr(low, SUMMARY_PERMANENT_QUOTA_MARKERS[i])) { hit = true; break; }
                WCUP_BRANCH(hit);                               /* B5 */
            } else {
                result = true;
            }
        }
        if (hit) result = true;
        free(low);
    }
done:
    {
        char cup[32];
        WCUP_VALSTR(result ? "True" : "False");                 /* V2 */
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("is_summary_access_or_quota_error");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_summary_access_or_quota_error <enc>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[1024];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *s1 = strchr(buf, '|');
        char *s2 = s1 ? strchr(s1 + 1, '|') : NULL;
        if (s1) *s1 = '\0';
        if (s2) *s2 = '\0';
        int code = 0;
        if (!strcmp(buf, "auth")) code = 35;
        else if (!strcmp(buf, "auth_permanent")) code = 36;
        else if (!strcmp(buf, "rate_limit")) code = 40;
        else if (!strcmp(buf, "billing")) code = 2;
        wc_cc_is_quota_error(code, s1 ? atoi(s1 + 1) : 0, s2 ? s2 + 1 : "");
    }
    return 0;
}
