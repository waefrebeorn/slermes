/* gate: hermes_cli_auth_py__get_nous_session_validity — traced copy of auth_get_nous_session_validity */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auth_get_nous_session_validity(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auth_get_nous_session_validity");
    int result = auth_get_nous_session_validity(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auth_get_nous_session_validity");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_get_nous_session_validity <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
