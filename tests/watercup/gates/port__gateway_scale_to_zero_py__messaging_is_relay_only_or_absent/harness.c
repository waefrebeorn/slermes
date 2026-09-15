/* gate: port__gateway_scale_to_zero_py__messaging_is_relay_only_or_absent — GAP sentinel for gateway/scale_to_zero.py:messaging_is_relay_only_or_absent */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static int gap_sentinel(const char *input) {
    (void)input;
    return 0;
}

static void trace_one(const char *input) {
    WCUP_BEGIN("messaging_is_relay_only_or_absent");
    int result = gap_sentinel(input);
#if 1
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", "messaging_is_relay_only_or_absent");
    WCUP_VALSTR(buf);
#else
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", (int)result);
    WCUP_VALSTR(buf);
#endif
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("messaging_is_relay_only_or_absent");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s messaging_is_relay_only_or_absent <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
