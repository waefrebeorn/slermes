/* gate: gateway_session_py___recover_session_from_db — traced copy of _recover_session_from_db */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _recover_session_from_db(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_recover_session_from_db");
    int result = _recover_session_from_db(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_recover_session_from_db");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _recover_session_from_db <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
