/* gate: hermes_cli_gateway_py__generate_launchd_plist — traced copy of cgw_generate_launchd_plist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_generate_launchd_plist(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_generate_launchd_plist");
    int result = cgw_generate_launchd_plist(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_generate_launchd_plist");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_generate_launchd_plist <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
