/* gate: hermes_cli_update_lock_py___pid_alive — traced copy of ul_pid_alive */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ul_pid_alive(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ul_pid_alive");
    int result = ul_pid_alive(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ul_pid_alive");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ul_pid_alive <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
