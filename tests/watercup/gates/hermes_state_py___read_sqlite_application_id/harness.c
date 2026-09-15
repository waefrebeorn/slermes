/* gate: hermes_state_py___read_sqlite_application_id — traced copy of _read_sqlite_application_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _read_sqlite_application_id(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_read_sqlite_application_id");
    int result = _read_sqlite_application_id(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_read_sqlite_application_id");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _read_sqlite_application_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
