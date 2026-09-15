/* gate: agent_transports_codex_py__validate_response — traced copy of tcx_validate_response */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tcx_validate_response(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tcx_validate_response");
    bool result = tcx_validate_response(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tcx_validate_response");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tcx_validate_response <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
