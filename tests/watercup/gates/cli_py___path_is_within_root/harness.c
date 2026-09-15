/* gate: cli_py___path_is_within_root — traced copy of _path_is_within_root */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _path_is_within_root(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_path_is_within_root");
    int result = _path_is_within_root(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_path_is_within_root");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _path_is_within_root <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
