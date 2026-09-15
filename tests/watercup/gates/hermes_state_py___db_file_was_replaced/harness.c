/* gate: hermes_state_py___db_file_was_replaced — traced copy of _db_file_was_replaced */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _db_file_was_replaced(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_db_file_was_replaced");
    int result = _db_file_was_replaced(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_db_file_was_replaced");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _db_file_was_replaced <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
