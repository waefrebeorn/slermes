/* gate: blueprints_schedule_to_string — traced C copy of
 * src/tools/blueprints.c::cli_tools_blueprints__schedule_to_string.
 * PoP: tools/blueprints.py:_schedule_to_string.
 * argv input per arg: "kind\texpr\tminutes\tseconds" ("-" = absent).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static const char *nz(const char *s) { return (!s || strcmp(s, "-") == 0) ? NULL : s; }

static void sched(const char *kind_in, const char *expr_in,
                  int minutes, int seconds, char *out, size_t outsz) {
    WCUP_BEGIN("schedule_to_string");
    const char *kind = nz(kind_in);
    const char *expr = nz(expr_in);
    if (kind && strcmp(kind, "cron") == 0 && expr) {
        WCUP_BRANCH(1);
        snprintf(out, outsz, "%s", expr);
    } else if (kind && strcmp(kind, "interval") == 0) {
        WCUP_BRANCH(1); /* interval rung taken */
        WCUP_BRANCH(0); /* cron rung not */
        if (minutes > 0) {
            WCUP_BRANCH(1);
            if (minutes % 60 == 0) {
                WCUP_BRANCH(1);
                snprintf(out, outsz, "every %dh", minutes / 60);
            } else {
                WCUP_BRANCH(0);
                snprintf(out, outsz, "every %dm", minutes);
            }
        } else if (seconds > 0) {
            WCUP_BRANCH(0);
            WCUP_BRANCH(1);
            if (seconds % 3600 == 0) {
                WCUP_BRANCH(1);
                snprintf(out, outsz, "every %dh", seconds / 3600);
            } else if (seconds % 60 == 0) {
                WCUP_BRANCH(1);
                snprintf(out, outsz, "every %dm", seconds / 60);
            } else {
                WCUP_BRANCH(0);
                snprintf(out, outsz, "every %ds", seconds);
            }
        } else {
            WCUP_BRANCH(0);
            WCUP_BRANCH(0);
            snprintf(out, outsz, "0 9 * * *");
        }
    } else if (expr) {
        WCUP_BRANCH(0);
        WCUP_BRANCH(1);
        snprintf(out, outsz, "%s", expr);
    } else {
        WCUP_BRANCH(0);
        WCUP_BRANCH(0);
        snprintf(out, outsz, "0 9 * * *");
    }
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("schedule_to_string");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s schedule_to_string <kind\texpr\tmin\tsec>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[256];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *kind = buf;
        char *t1 = strchr(buf, '\t');
        if (t1) { *t1 = '\0'; }
        char *expr = t1 ? t1 + 1 : NULL;
        char *t2 = expr ? strchr(expr, '\t') : NULL;
        if (t2) { *t2 = '\0'; }
        char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
        if (t3) { *t3 = '\0'; }
        int minutes = t2 ? atoi(t2 + 1) : 0;
        int seconds = t3 ? atoi(t3 + 1) : 0;
        char out[128];
        sched(kind, expr, minutes, seconds, out, sizeof out);
    }
    return 0;
}
