/* gate: gateway_dead_targets_py__is_dead_error_kind — traced copy of dead_target_is_dead_error_kind */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool dead_target_is_dead_error_kind(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("dead_target_is_dead_error_kind");
    bool result = dead_target_is_dead_error_kind(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dead_target_is_dead_error_kind");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dead_target_is_dead_error_kind <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
