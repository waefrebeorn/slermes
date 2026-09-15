/* gate: agent_ssl_guard_py___validate_bundle_path — traced copy of sslg_validate_bundle_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sslg_validate_bundle_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sslg_validate_bundle_path");
    int result = sslg_validate_bundle_path(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sslg_validate_bundle_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sslg_validate_bundle_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
