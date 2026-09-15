/* gate: gateway_run_py___make_profile_busy_session_handler — traced copy of gw_make_profile_busy_session_handler */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_make_profile_busy_session_handler(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_make_profile_busy_session_handler");
    int result = gw_make_profile_busy_session_handler(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_make_profile_busy_session_handler");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_make_profile_busy_session_handler <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
