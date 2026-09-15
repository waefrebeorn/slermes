/* gate: gateway_status_py___clear_running_pid_cache — traced copy of gstat_u_clear_running_pid_cache */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gstat_u_clear_running_pid_cache(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gstat_u_clear_running_pid_cache");
    int result = gstat_u_clear_running_pid_cache(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gstat_u_clear_running_pid_cache");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gstat_u_clear_running_pid_cache <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
