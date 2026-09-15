/* gate: hermes_cli_auth_py___same_path — traced copy of authstore_same_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int authstore_same_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("authstore_same_path");
    int result = authstore_same_path(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("authstore_same_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s authstore_same_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
