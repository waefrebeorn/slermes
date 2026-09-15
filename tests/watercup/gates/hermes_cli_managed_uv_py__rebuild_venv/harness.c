/* gate: hermes_cli_managed_uv_py__rebuild_venv — traced copy of muv_rebuild_venv */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int muv_rebuild_venv(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("muv_rebuild_venv");
    int result = muv_rebuild_venv(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("muv_rebuild_venv");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s muv_rebuild_venv <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
