/* gate: hermes_state_py___wal_generation_was_lost — traced copy of _wal_generation_was_lost */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _wal_generation_was_lost(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_wal_generation_was_lost");
    int result = _wal_generation_was_lost(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_wal_generation_was_lost");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _wal_generation_was_lost <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
