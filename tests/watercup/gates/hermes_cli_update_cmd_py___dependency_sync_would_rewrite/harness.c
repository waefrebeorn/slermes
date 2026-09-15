/* gate: hermes_cli_update_cmd_py___dependency_sync_would_rewrite — traced copy of _dependency_sync_would_rewrite */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _dependency_sync_would_rewrite(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_dependency_sync_would_rewrite");
    int result = _dependency_sync_would_rewrite(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_dependency_sync_would_rewrite");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _dependency_sync_would_rewrite <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
