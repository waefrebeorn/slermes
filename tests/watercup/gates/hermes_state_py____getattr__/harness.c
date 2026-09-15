/* gate: hermes_state_py____getattr__ — traced copy of hermes_state_getattr_offload */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_state_getattr_offload(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_state_getattr_offload");
    int result = hermes_state_getattr_offload(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_state_getattr_offload");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_state_getattr_offload <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
