/* gate: hermes_cli_update_lock_py___is_ancestor_pid — traced copy of ul_is_ancestor_pid */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ul_is_ancestor_pid(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ul_is_ancestor_pid");
    int result = ul_is_ancestor_pid(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ul_is_ancestor_pid");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ul_is_ancestor_pid <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
