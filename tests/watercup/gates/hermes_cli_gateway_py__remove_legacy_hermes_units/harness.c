/* gate: hermes_cli_gateway_py__remove_legacy_hermes_units — traced copy of cgw_remove_legacy_hermes_units */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_remove_legacy_hermes_units(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_remove_legacy_hermes_units");
    int result = cgw_remove_legacy_hermes_units(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_remove_legacy_hermes_units");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_remove_legacy_hermes_units <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
