/* gate: libcron_next_run — drives the REAL lib/libcron/cron.c
 * (cron_parse + cron_next) over expression/time probes.
 * LIVE twin: croniter (the library LIVE hermes_cli/cron.py schedules with).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: expression is @-special (@hourly/@daily/...)
 *   B2: parse failed -> invalid
 *   B3: next matching time found within horizon
 *   V1: next-run epoch (UTC), -1 when not found
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "watercup.h"
#include "cron.h"

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_cron_next <expr|from_epoch>...\n", argv[0]); return 2; }
    setenv("TZ", "UTC", 1);
    tzset();

    for (int i = 2; i < argc; i++) {
        char probe[512];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *bar = strrchr(probe, '|');
        time_t from_t = bar ? (time_t)strtoll(bar + 1, NULL, 10) : 0;
        if (bar) *bar = '\0';
        const char *expr = probe;

        WCUP_BEGIN("cron_next_run");
        /* B1: @-special */
        if (expr[0] == '@' && strncmp(expr, "@every ", 7) != 0) {
            WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0);
        }

        char errbuf[128];
        char *err = errbuf;
        cron_expr_t *c = cron_parse(expr, &err);

        if (!c) {                                           /* B2 */
            WCUP_BRANCH(1);
            WCUP_VALSTR("-1");
            char cup[32];
            WCUP_FINALIZE(cup, sizeof cup);
            printf("%s\n", cup);
            WCUP_END("cron_next_run");
            continue;
        }
        WCUP_BRANCH(0);

        struct tm from_tm, next_tm;
        localtime_r(&from_t, &from_tm);
        bool found = cron_next(c, &from_tm, &next_tm);      /* B3 */
        WCUP_BRANCH(found);

        char vs[32];
        if (found) {
            time_t nt = mktime(&next_tm);
            snprintf(vs, sizeof vs, "%lld", (long long)nt);
        } else {
            snprintf(vs, sizeof vs, "-1");
        }
        WCUP_VALSTR(vs);

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cron_next_run");
        cron_free(c);
    }
    return 0;
}
