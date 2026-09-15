/* gate: hermes_cli_agent_plugins_py___validate_headers — traced copy of ap_validate_headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ap_validate_headers(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ap_validate_headers");
    int result = ap_validate_headers(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ap_validate_headers");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ap_validate_headers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
