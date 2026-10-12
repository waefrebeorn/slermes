/* gate: real__hermes_cli_tools_config_py___exempt_explicit_platform_native — GAP sentinel for _exempt_explicit_platform_native */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_exempt_explicit_platform_native";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _exempt_explicit_platform_native <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_exempt_explicit_platform_native");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_exempt_explicit_platform_native");
    }
    return 0;
}
