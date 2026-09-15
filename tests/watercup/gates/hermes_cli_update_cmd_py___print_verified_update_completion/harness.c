/* gate: hermes_cli_update_cmd_py___print_verified_update_completion — traced copy of _print_verified_update_completion */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _print_verified_update_completion(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_print_verified_update_completion");
    int result = _print_verified_update_completion(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_print_verified_update_completion");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _print_verified_update_completion <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
