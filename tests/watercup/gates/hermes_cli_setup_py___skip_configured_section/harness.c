/* gate: hermes_cli_setup_py___skip_configured_section — traced copy of _skip_configured_section */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _skip_configured_section(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_skip_configured_section");
    int result = _skip_configured_section(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_skip_configured_section");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _skip_configured_section <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
