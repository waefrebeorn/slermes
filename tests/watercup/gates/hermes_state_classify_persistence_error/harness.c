/* gate: hermes_state_classify_persistence_error — traced copy of the FIXED
 * src/agent/hermes_state_gaps.c::hs_classify_persistence_error
 * (LIVE twin: hermes_state.py::classify_persistence_error, string inputs).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: input is None
 *   B2-B4: typed-exception isinstance checks (false for string inputs)
 *   B5: "turn lease"          B6: "closed by compression"
 *   B7: "being compressed"    B8: "compression lease"
 *   B9-B12: corruption markers (malformed / file is not a database /
 *           not a database / database corruption)
 *   B13: locked   B14: busy
 *   B15-B20: disk-full markers
 *   B21: plain "disk"  B22: readonly  B23: read-only
 *   VSTR(bucket)
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

static const char *const CORRUPTION_MARKERS[] = {
    "malformed", "file is not a database", "not a database",
    "database corruption", NULL
};
static const char *const DISK_FULL_MARKERS[] = {
    "no space left on device", "not enough space", "database or disk is full",
    "disk full", "full disk", "enospc", NULL
};

static const char *wc_classify(const char *errmsg)
{
    const char *label = "unknown";
    WCUP_BEGIN("classify_persistence_error");

    WCUP_BRANCH(errmsg == NULL);                    /* B1 None */
    if (!errmsg) goto done;
    /* B2-B4: typed exception checks (string inputs => false) */
    WCUP_BRANCH(0);
    WCUP_BRANCH(0);
    WCUP_BRANCH(0);

    {
        char lowered[1024];
        size_t i = 0;
        for (; errmsg[i] && i < sizeof(lowered) - 1; i++)
            lowered[i] = (char)tolower((unsigned char)errmsg[i]);
        lowered[i] = '\0';

        int b5 = strstr(lowered, "turn lease") != NULL;
        WCUP_BRANCH(b5);                            /* B5 */
        if (b5) { label = "turn_lease"; goto done; }

        int b6 = strstr(lowered, "closed by compression") != NULL;
        WCUP_BRANCH(b6);                            /* B6 */
        if (b6) { label = "compression_closed"; goto done; }

        int b7 = strstr(lowered, "being compressed") != NULL;
        int b8 = strstr(lowered, "compression lease") != NULL;
        WCUP_BRANCH(b7);                            /* B7 */
        WCUP_BRANCH(b8);                            /* B8 */
        if (b7 || b8) { label = "compression"; goto done; }

        for (int m = 0; CORRUPTION_MARKERS[m]; m++) {   /* B9-B12 */
            int hit = strstr(lowered, CORRUPTION_MARKERS[m]) != NULL;
            WCUP_BRANCH(hit);
            if (hit) { label = "corrupt"; goto done; }
        }

        int b13 = strstr(lowered, "locked") != NULL;
        int b14 = strstr(lowered, "busy") != NULL;
        WCUP_BRANCH(b13);                           /* B13 */
        WCUP_BRANCH(b14);                           /* B14 */
        if (b13 || b14) { label = "locked"; goto done; }

        int disk = 0;
        for (int m = 0; DISK_FULL_MARKERS[m]; m++) {    /* B15-B20 */
            int hit = strstr(lowered, DISK_FULL_MARKERS[m]) != NULL;
            WCUP_BRANCH(hit);
            if (hit) { disk = 1; break; }
        }
        if (disk) {
            WCUP_BRANCH(1);                         /* B21 short-circuit true */
            WCUP_BRANCH(1);                         /* B22 */
            WCUP_BRANCH(1);                         /* B23 */
        } else {
            int b21 = strstr(lowered, "disk") != NULL;
            WCUP_BRANCH(b21);
            int b22 = !b21 && strstr(lowered, "readonly") != NULL;
            WCUP_BRANCH(b22);
            int b23 = !b21 && !b22 && strstr(lowered, "read-only") != NULL;
            WCUP_BRANCH(b23);
            disk = b21 || b22 || b23;
        }
        if (disk) label = "disk";
    }

done:
    WCUP_VALSTR(label);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
    }
    WCUP_END("classify_persistence_error");
    return label;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s fn <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        wc_classify(argv[i]);
    return 0;
}
