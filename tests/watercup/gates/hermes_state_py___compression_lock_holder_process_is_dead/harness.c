/* gate: hermes_state_py___compression_lock_holder_process_is_dead — traced copy of hermes_state_lock_holder_process_is_dead */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool hermes_state_lock_holder_process_is_dead(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_state_lock_holder_process_is_dead");
    bool result = hermes_state_lock_holder_process_is_dead(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_state_lock_holder_process_is_dead");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_state_lock_holder_process_is_dead <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
