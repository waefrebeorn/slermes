/* gate: loops_format_interval — traced C copy of
 * src/hermes_cli/loops_pure.c::loops_format_interval (LIVE twin:
 * hermes_cli/loops.py::format_interval).
 *
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: h != 0        -> append "<h>h"
 *   B2: m != 0        -> append "<m>m"
 *   B3: s != 0 OR no parts yet -> append "<s>s" (else-case covers "0s")
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static char *loops_format_interval(double seconds, char *buf, size_t bufsz) {
    WCUP_BEGIN("loops_format_interval");
    if (seconds < 0) seconds = 0;
    long secs = (long)(seconds + 0.5);
    long h = secs / 3600;
    long rem = secs % 3600;
    long mm = rem / 60;
    long ss = rem % 60;
    buf[0] = '\0';
    if (h) {
        WCUP_BRANCH(1);
        char t[16];
        snprintf(t, sizeof(t), "%ldh", h);
        strcat(buf, t);
    } else {
        WCUP_BRANCH(0);
    }
    if (mm) {
        WCUP_BRANCH(1);
        char t[16];
        snprintf(t, sizeof(t), "%ldm", mm);
        strcat(buf, t);
    } else {
        WCUP_BRANCH(0);
    }
    if (ss || buf[0] == '\0') {
        WCUP_BRANCH(1);
        char t[16];
        snprintf(t, sizeof(t), "%lds", ss);
        strcat(buf, t);
    } else {
        WCUP_BRANCH(0);
    }
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("loops_format_interval");
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s loops_format_interval <secs>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[64];
        loops_format_interval(strtod(argv[i], NULL), buf, sizeof buf);
    }
    return 0;
}
