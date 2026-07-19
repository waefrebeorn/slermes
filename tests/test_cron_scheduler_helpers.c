/*
 * test_cron_scheduler_helpers.c — unit tests for the pure cron scheduler
 * string helpers (src/cron/port_cron_scheduler_helpers.c).
 * Expected strings derived from cron/scheduler.py:_summarize_cron_failure_for_delivery.
 */

#include "cron_scheduler_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WARN "\xe2\x9a\xa0\xef\xb8\x8f"  /* ⚠️ */

static int g_fail = 0;

static void check_sum(const char *jn, const char *err, const char *expected)
{
    char *got = scheduler_summarize_cron_failure(jn, err);
    if (!got || strcmp(got, expected) != 0) {
        printf("FAIL: summarize(%s, %s)\n  expected=[%s]\n  got    =[%s]\n",
               jn ? jn : "NULL", err ? err : "NULL", expected, got ? got : "(null)");
        g_fail++;
    } else {
        printf("ok: summarize(%s, %s)\n", jn ? jn : "NULL", err ? err : "NULL");
    }
    free(got);
}

static void check_silence(const char *text, int expected)
{
    int got = scheduler_is_cron_silence_response(text);
    if (got != expected) {
        printf("FAIL: silence(%s) expected=%d got=%d\n", text, expected, got);
        g_fail++;
    } else {
        printf("ok: silence(%s)=%d\n", text, got);
    }
}

int main(void)
{
    /* _summarize_cron_failure_for_delivery */
    check_sum("nightly", "HTTP 429 Too Many Requests: rate limited by provider",
        WARN " Cron 'nightly' failed: provider rate limit. "
        "Fallback chain was exhausted or unavailable. Full details saved in cron output.");

    check_sum("nightly", "weekly usage limit exceeded",
        WARN " Cron 'nightly' failed: provider weekly usage limit. "
        "Fallback chain was exhausted or unavailable. Full details saved in cron output.");

    check_sum("nightly", "quota exhausted",
        WARN " Cron 'nightly' failed: quota exhausted");

    check_sum("nightly", "ReadTimeout: the request timed out",
        WARN " Cron 'nightly' failed: provider timeout. "
        "Fallback chain was exhausted or unavailable. Full details saved in cron output.");

    check_sum("nightly", "provider timeout after 30s",
        WARN " Cron 'nightly' failed: provider timeout. "
        "Fallback chain was exhausted or unavailable. Full details saved in cron output.");

    check_sum("nightly", "AuthenticationError: invalid token",
        WARN " Cron 'nightly' failed: provider authentication error. Full details saved in cron output.");

    check_sum("nightly", "403 Forbidden from API",
        WARN " Cron 'nightly' failed: provider authentication error. Full details saved in cron output.");

    /* 4015 must NOT trip the whole-word 401 pattern */
    check_sum("nightly", "4015 is a weird code not auth",
        WARN " Cron 'nightly' failed: 4015 is a weird code not auth");

    check_sum("nightly", "ValueError: something broke in step 3 of the pipeline",
        WARN " Cron 'nightly' failed: something broke in step 3 of the pipeline");

    /* long generic text -> truncated + "..." */
    {
        char big[300];
        memset(big, 'x', sizeof(big) - 1);
        big[sizeof(big) - 1] = '\0';
        /* Python: cleaned[:177].rstrip() + "..." */
        char body[200];
        memset(body, 'x', 177);
        body[177] = '\0';
        char expected[400];
        snprintf(expected, sizeof(expected),
            WARN " Cron 'nightly' failed: %s...", body);
        check_sum("nightly", big, expected);
    }

    check_sum(NULL, NULL,
        WARN " Cron 'cron job' failed: unknown error");

    check_sum("my job", "unknown error",
        WARN " Cron 'my job' failed: unknown error");

    /* _is_cron_silence_response */
    check_silence("[SILENT]", 1);
    check_silence("SILENT", 1);
    check_silence("NO_REPLY", 1);
    check_silence("NO REPLY", 1);
    check_silence("2 deals filtered\n\n[SILENT]", 1);
    check_silence("[SILENT] No changes detected", 1);
    check_silence("Silent retry succeeded", 0);   /* bracketless word must NOT swallow */
    check_silence("all good", 0);
    check_silence("", 0);
    check_silence(NULL, 0);

    /* normalize_deliver_value */
    {
        char *d = scheduler_normalize_deliver_value("");
        if (!d || strcmp(d, "local") != 0) { printf("FAIL: normalize empty\n"); g_fail++; }
        else printf("ok: normalize empty=local\n");
        free(d);
        d = scheduler_normalize_deliver_value("telegram");
        if (!d || strcmp(d, "telegram") != 0) { printf("FAIL: normalize scalar\n"); g_fail++; }
        else printf("ok: normalize scalar=telegram\n");
        free(d);
    }

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
