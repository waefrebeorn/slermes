/* gate: real__hermes_cli_gateway_py___running_under_gateway_supervisor — GAP sentinel for _running_under_gateway_supervisor */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_running_under_gateway_supervisor";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _running_under_gateway_supervisor <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_running_under_gateway_supervisor");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_running_under_gateway_supervisor");
    }
    return 0;
}
