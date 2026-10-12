/* gate: real__hermes_cli_gateway_py___build_service_path_dirs — GAP sentinel for _build_service_path_dirs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_build_service_path_dirs";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _build_service_path_dirs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_build_service_path_dirs");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_build_service_path_dirs");
    }
    return 0;
}
