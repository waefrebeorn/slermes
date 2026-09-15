/* gate: hermes_cli_backup_py___count_session_rows — traced copy of _count_session_rows */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _count_session_rows(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_count_session_rows");
    int result = _count_session_rows(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_count_session_rows");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _count_session_rows <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
