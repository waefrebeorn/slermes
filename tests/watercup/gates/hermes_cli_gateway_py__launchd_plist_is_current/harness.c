/* gate: hermes_cli_gateway_py__launchd_plist_is_current — traced copy of cgw_launchd_plist_is_current */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_launchd_plist_is_current(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_launchd_plist_is_current");
    int result = cgw_launchd_plist_is_current(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_launchd_plist_is_current");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_launchd_plist_is_current <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
