/* gate: gateway_dead_targets_py____init__ — traced copy of gw_dead_targets_init */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_dead_targets_init(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_dead_targets_init");
    int result = gw_dead_targets_init(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_dead_targets_init");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_dead_targets_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
