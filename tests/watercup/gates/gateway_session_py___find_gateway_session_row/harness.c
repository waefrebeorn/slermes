/* gate: gateway_session_py___find_gateway_session_row — traced copy of session_find_by_key */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int session_find_by_key(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("session_find_by_key");
    int result = session_find_by_key(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("session_find_by_key");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s session_find_by_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
