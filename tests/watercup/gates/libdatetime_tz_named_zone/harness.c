/* gate: libdatetime_tz_named_zone — drives the REAL datetime_format_tz()
 * (lib/libdatetime/datetime.c) over zone|epoch probes.
 *
 * Why this gate exists: the port used to set TZ and call tzset() to render a named
 * zone. On Windows the CRT does not honour TZ (measured: TZ=America/New_York gave
 * -3600 where EDT is -14400, and TZ=UTC was ignored outright), so every named-zone
 * render silently produced the machine wall clock. The zone table replaced that;
 * this gate keeps it honest against LIVE zoneinfo.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: the zone is a named zone the port knows (a table hit)
 *   B2: formatting succeeded
 *   V1: "%Y-%m-%d %H:%M" in that zone, or "-" on failure
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "watercup.h"
#include "datetime.h"

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_tz_format <zone|epoch>...\n", argv[0]); return 2; }

    for (int i = 2; i < argc; i++) {
        char probe[256];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *bar = strrchr(probe, '|');
        time_t ts = bar ? (time_t)strtoll(bar + 1, NULL, 10) : 0;
        if (bar) *bar = '\0';
        const char *zone = probe;

        WCUP_BEGIN("tz_named_zone");

        /* B1: known zone. The port exposes its table indirectly: an unknown zone
         * still formats (through the TZ fallback), so the rung is decided by the
         * offset the port reports against UTC, which is 0 only for zones it maps
         * itself... instead of guessing, ask the documented helper. */
        int known = datetime_zone_is_known(zone);
        WCUP_BRANCH(known);

        char out[64] = "-";
        char *got = datetime_format_tz(ts, zone, "%Y-%m-%d %H:%M");
        int ok = (got != NULL);
        WCUP_BRANCH(ok);
        if (ok) {
            snprintf(out, sizeof out, "%s", got);
            free(got);
        }
        WCUP_VALSTR(out);

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tz_named_zone");
    }
    return 0;
}
