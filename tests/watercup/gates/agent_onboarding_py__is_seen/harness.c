/* gate: agent_onboarding_py__is_seen — traced copy of obd_is_seen */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int obd_is_seen(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("obd_is_seen");
    int result = obd_is_seen(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("obd_is_seen");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s obd_is_seen <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
