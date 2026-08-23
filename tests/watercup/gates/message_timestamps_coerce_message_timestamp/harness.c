/* gate: message_timestamps_coerce_message_timestamp — traced faithful copy of
 * src/gateway/message_timestamps_ports.c::mts_coerce_message_timestamp.
 * LIVE twin: gateway/message_timestamps.py::coerce_message_timestamp.
 *
 * Agreed sub-contract (see gate.json notes): positive integer epochs and
 * explicit-UTC ISO timestamps; blanks and junk fail closed on both sides.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include "watercup.h"

static long coerce(const char *value) {
    /* real port logic */
    if (!value || !*value) return -1;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end != value && *end == '\0' && v > 0) return v;
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    if (sscanf(value, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) >= 3) {
        struct tm t;
        memset(&t, 0, sizeof(t));
        t.tm_year = y - 1900;
        t.tm_mon = mo - 1;
        t.tm_mday = d;
        t.tm_hour = h;
        t.tm_min = mi;
        t.tm_sec = s;
        time_t tt = timegm(&t);
        if (tt > 0) return (long)tt;
    }
    return -1;
}

static void trace_one(const char *raw) {
    WCUP_BEGIN("coerce_message_timestamp");
    /* b0: blank-after-strip (LIVE strips first; the port's empty check plus
     * its whitespace-only strtol failure agree on the same outcome) */
    const char *p = raw;
    while (*p == ' ' || *p == '\t' || *p == '\n') p++;
    bool blank = *p == '\0';
    WCUP_BRANCH(blank);
    /* b1: pure positive-integer epoch (full strtol parse, v>0) */
    bool is_int = false;
    long iv = 0;
    {
        char *end = NULL;
        iv = strtol(raw, &end, 10);
        is_int = (!blank && end != raw && *end == '\0' && iv > 0);
    }
    WCUP_BRANCH(is_int);
    /* b2: ISO datetime shape (>=3 sscanf conversions) */
    int y=0,mo=0,d=0,h=0,mi=0,s=0;
    bool iso_ok = !blank && !is_int &&
        sscanf(raw, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) >= 3;
    WCUP_BRANCH(iso_ok);
    long r = coerce(raw);
    char vbuf[64];
    if (r < 0) snprintf(vbuf, sizeof vbuf, "none");
    else snprintf(vbuf, sizeof vbuf, "%.17g", (double)r);
    WCUP_VALSTR(vbuf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("coerce_message_timestamp");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s coerce_message_timestamp <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
