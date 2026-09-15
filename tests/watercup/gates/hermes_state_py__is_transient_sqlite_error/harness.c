/* gate: hermes_state_py__is_transient_sqlite_error — traced copy of is_transient_sqlite_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int is_transient_sqlite_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_transient_sqlite_error");
    int result = is_transient_sqlite_error(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_transient_sqlite_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_transient_sqlite_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
