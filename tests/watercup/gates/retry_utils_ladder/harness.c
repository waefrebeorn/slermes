/* gate: retry_utils_ladder — traced copies of
 *   src/agent/retry_utils.c::retry_utils_parse_retry_after_seconds
 *   src/agent/retry_utils.c::is_zai_coding_overload_error
 *   src/agent/retry_utils.c::zai_coding_overload_retry_ceiling
 * (LIVE twin: agent/retry_utils.py).
 *
 * Input tags (argv):
 *   "ra:<value>"                        parse_retry_after_seconds
 *   "zai:<status>|<base>|<model>|<text>" is_zai_coding_overload_error
 *   "ceil:<n>"                          zai_coding_overload_retry_ceiling
 *
 * Branch ladder ra: (must match twin.py EXACTLY)
 *   B1: stripped value empty
 *   B2: numeric seconds parsed
 *   B3: HTTP-date parsed
 *   VSTR(label none|secs|date|invalid)
 * Branch ladder zai:
 *   B1 status==429 / B2 base match / B3 model match / B4 text match
 *   VSTR("1"/"0")
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "watercup.h"

static int wc_parse_date(const char *s, struct tm *tm)
{
    memset(tm, 0, sizeof(*tm));
    return strptime(s, "%a, %d %b %Y %H:%M:%S %Z", tm) ? 0 : -1;
}

static void wc_ra(const char *value)
{
    WCUP_BEGIN("retry_parse_retry_after");
    const char *label = "invalid";

    if (!value || !*value) { WCUP_BRANCH(1); label = "none"; goto done; }
    WCUP_BRANCH(0);

    {
        const char *start = value;
        while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
        size_t slen = strlen(start);
        while (slen > 0 && (start[slen-1] == ' ' || start[slen-1] == '\t' ||
                            start[slen-1] == '\r' || start[slen-1] == '\n'))
            slen--;
        if (slen == 0) { WCUP_BRANCH(1); label = "none"; goto done; }
        WCUP_BRANCH(0);

        char buf[256];
        if (slen >= sizeof buf) slen = sizeof buf - 1;
        memcpy(buf, start, slen); buf[slen] = '\0';

        char *end = NULL;
        double seconds = strtod(buf, &end);
        if (end != buf && *end == '\0') {
            WCUP_BRANCH(1); label = "secs"; goto done;
        }
        WCUP_BRANCH(0);

        struct tm tm;
        if (wc_parse_date(buf, &tm) == 0 && timegm(&tm) != (time_t)-1) {
            WCUP_BRANCH(1); label = "date"; goto done;
        }
        WCUP_BRANCH(0);
    }

done:
    WCUP_VALSTR(label);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("retry_parse_retry_after");
}

static void wc_zai(int status_code, const char *base_url, const char *model,
                   const char *text)
{
    WCUP_BEGIN("retry_zai_overload");

    WCUP_BRANCH(status_code == 429);
    if (status_code != 429) { WCUP_VALSTR("0"); goto print; }
    {
        char low[2048];
        size_t n = 0;
        for (const char *p = base_url ? base_url : ""; *p && n + 1 < sizeof(low); p++)
            low[n++] = (char)tolower((unsigned char)*p);
        low[n] = '\0';
        int hit = strstr(low, "api.z.ai/api/coding/paas/v4") != NULL;
        WCUP_BRANCH(hit);
        if (!hit) { WCUP_VALSTR("0"); goto print; }
    }
    {
        char low[1024];
        size_t n = 0;
        for (const char *p = model ? model : ""; *p && n + 1 < sizeof(low); p++)
            low[n++] = (char)tolower((unsigned char)*p);
        low[n] = '\0';
        int hit = strstr(low, "glm-5.2") != NULL;
        WCUP_BRANCH(hit);
        if (!hit) { WCUP_VALSTR("0"); goto print; }
    }
    {
        char low[4096];
        size_t n = 0;
        for (const char *p = text ? text : ""; *p && n + 1 < sizeof(low); p++)
            low[n++] = (char)tolower((unsigned char)*p);
        low[n] = '\0';
        int hit = strstr(low, "1305") || strstr(low, "temporarily overloaded");
        WCUP_BRANCH(hit);
        WCUP_VALSTR(hit ? "1" : "0");
    }
print:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("retry_zai_overload");
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s fn <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *in = argv[i];
        if (strncmp(in, "ra:", 3) == 0) {
            wc_ra(in + 3);
        } else if (strncmp(in, "zai:", 4) == 0) {
            char fields[3][512];
            const char *p = in + 4;
            for (int f = 0; f < 3; f++) {
                const char *bar = strchr(p, '|');
                size_t len = bar ? (size_t)(bar - p) : strlen(p);
                if (len >= sizeof fields[0]) len = sizeof fields[0] - 1;
                memcpy(fields[f], p, len); fields[f][len] = '\0';
                p = bar ? bar + 1 : p + strlen(p);
            }
            int status = atoi(fields[0]);
            wc_zai(status, fields[1], fields[2], p);
        } else if (strncmp(in, "ceil:", 5) == 0) {
            WCUP_BEGIN("retry_zai_ceiling");
            int n = atoi(in + 5);
            int ceiling = n + 4 + 1;   /* short_attempts + len(LONG_BACKOFF) + 1 */
            char res[16];
            snprintf(res, sizeof res, "%d", ceiling);
            WCUP_VALSTR(res);
            char cup[32];
            WCUP_FINALIZE(cup, sizeof cup);
            printf("%s\n", cup);
            WCUP_END("retry_zai_ceiling");
        } else {
            fprintf(stderr, "bad input: %s\n", in);
            return 2;
        }
    }
    return 0;
}
