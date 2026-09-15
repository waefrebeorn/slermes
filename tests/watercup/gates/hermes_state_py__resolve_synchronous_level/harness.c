/* gate: hermes_state_py__resolve_synchronous_level — traced copy of resolve_synchronous_level */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int resolve_synchronous_level(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("resolve_synchronous_level");
    int result = resolve_synchronous_level(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("resolve_synchronous_level");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_synchronous_level <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
