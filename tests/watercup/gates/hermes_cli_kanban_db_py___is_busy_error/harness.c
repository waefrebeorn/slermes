/* gate: hermes_cli_kanban_db_py___is_busy_error — traced copy of kdb_is_busy_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdb_is_busy_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdb_is_busy_error");
    int result = kdb_is_busy_error(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdb_is_busy_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdb_is_busy_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
