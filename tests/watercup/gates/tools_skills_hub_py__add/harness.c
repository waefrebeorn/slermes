/* gate: tools_skills_hub_py__add — traced copy of hub_taps_add */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hub_taps_add(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("hub_taps_add");
    int result = hub_taps_add(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hub_taps_add");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hub_taps_add <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
