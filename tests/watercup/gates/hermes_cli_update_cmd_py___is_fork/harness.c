/* gate: hermes_cli_update_cmd_py___is_fork — traced copy of uc_is_fork_origin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool uc_is_fork_origin(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("uc_is_fork_origin");
    bool result = uc_is_fork_origin(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uc_is_fork_origin");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_is_fork_origin <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
