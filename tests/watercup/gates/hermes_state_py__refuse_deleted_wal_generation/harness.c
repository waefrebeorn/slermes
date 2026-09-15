/* gate: hermes_state_py__refuse_deleted_wal_generation — traced copy of refuse_deleted_wal_generation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool refuse_deleted_wal_generation(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("refuse_deleted_wal_generation");
    bool result = refuse_deleted_wal_generation(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("refuse_deleted_wal_generation");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s refuse_deleted_wal_generation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
