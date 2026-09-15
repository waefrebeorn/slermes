/* gate: hermes_cli_service_manager_py__is_running — traced copy of sm_is_running */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sm_is_running(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sm_is_running");
    int result = sm_is_running(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sm_is_running");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sm_is_running <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
