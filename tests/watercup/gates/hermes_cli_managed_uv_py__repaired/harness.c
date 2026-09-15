/* gate: hermes_cli_managed_uv_py__repaired — traced copy of muv_repaired */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int muv_repaired(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("muv_repaired");
    int result = muv_repaired(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("muv_repaired");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s muv_repaired <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
