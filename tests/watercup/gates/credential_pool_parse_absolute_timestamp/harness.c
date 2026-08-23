/* gate: credential_pool_parse_absolute_timestamp — traced copy of
 * src/agent/credential_pool_ports.c::cpl_parse_absolute_timestamp
 * (PoP: agent/credential_pool.py:_parse_absolute_timestamp).
 *
 * Rungs: null/empty -> strtod-full-parse? -> >1e12 (ms) -> ISO sscanf? -> result.
 * Sentinel -1 == LIVE None. %.17g value events on the numeric result.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "watercup.h"

static void parse_absolute_timestamp(const char *value) {
    WCUP_BEGIN("parse_absolute_timestamp");
    char cls[64];
    double result = -1;
    if (!value || !*value) {
        WCUP_BRANCH(1);
        snprintf(cls, sizeof cls, "none");
    } else {
        WCUP_BRANCH(0);
        char *end = NULL;
        double v = strtod(value, &end);
        if (end != value && *end == '\0') {
            WCUP_BRANCH(1);
            if (v > 1e12) { WCUP_BRANCH(1); v /= 1000.0; }
            else WCUP_BRANCH(0);
            result = v;
            snprintf(cls, sizeof cls, "num");
        } else {
            WCUP_BRANCH(0);
            struct tm tm;
            memset(&tm, 0, sizeof tm);
            if (sscanf(value, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                       &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
                WCUP_BRANCH(1);
                tm.tm_year -= 1900; tm.tm_mon -= 1;
                result = (double)timegm(&tm);
                snprintf(cls, sizeof cls, "iso");
            } else {
                WCUP_BRANCH(0);
                snprintf(cls, sizeof cls, "none");
            }
        }
    }
    WCUP_VALSTR(cls);
    char vbuf[64];
    snprintf(vbuf, sizeof vbuf, "%.17g", result);
    WCUP_VALSTR(vbuf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("parse_absolute_timestamp");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_absolute_timestamp <val>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) parse_absolute_timestamp(argv[i]);
    return 0;
}
