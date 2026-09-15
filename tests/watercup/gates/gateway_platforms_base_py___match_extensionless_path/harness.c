/* gate: gateway_platforms_base_py___match_extensionless_path — traced copy of stts_match_extensionless_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int stts_match_extensionless_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("stts_match_extensionless_path");
    int result = stts_match_extensionless_path(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("stts_match_extensionless_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stts_match_extensionless_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
