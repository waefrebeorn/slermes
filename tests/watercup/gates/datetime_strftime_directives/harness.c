/* gate: datetime_strftime_directives — traced driver over the REAL
 * lib/libdatetime/datetime.c::datetime_format (LIVE twin:
 * python datetime.fromtimestamp(ts, timezone.utc).strftime).
 *
 * Probe encoding: "<epoch-seconds>|<strftime-format>"
 *
 * Rung order (must match twin.py EXACTLY): walk the format string;
 *   D1: char is '%'
 *   D2: directive is in the covered set
 *   V: formatted result from the REAL datetime_format
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"
#include "libdatetime/datetime.h"

static int covered(char d) {
    switch (d) {
    case 'Y': case 'm': case 'd': case 'H': case 'M': case 'S':
    case 'y': case 'j': case 'A': case 'a': case 'B': case 'b':
    case 'p': case 'I': case 'U': case 'W': case 'Z': case 'z':
    case '%': case 'F': case 'T': case 'e':
        return 1;
    default:
        return 0;
    }
}

int main(int argc, char **argv) {
    setenv("TZ", "UTC", 1);
    tzset();
    if (argc < 3) { fprintf(stderr, "usage: %s fn <ts|fmt>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char probe[512];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *p = probe;
        char *ts_s = strsep(&p, "|");
        const char *fmt = p ? p : "";
        long long ts = ts_s ? atoll(ts_s) : 0;

        WCUP_BEGIN("datetime_strftime_directives");

        for (const char *q = fmt; *q; q++) {
            if (*q == '%') {                                  /* D1 */
                WCUP_BRANCH(1);
                q++;
                if (covered(*q)) WCUP_BRANCH(1);              /* D2 */
                else             WCUP_BRANCH(0);
            } else {
                WCUP_BRANCH(0);
            }
        }

        char *out = datetime_format((time_t)ts, fmt);
        WCUP_VALSTR(out ? out : "");
        free(out);

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("datetime_strftime_directives");
    }
    return 0;
}
