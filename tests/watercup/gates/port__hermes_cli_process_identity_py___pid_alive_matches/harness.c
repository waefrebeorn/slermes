/* gate: port__hermes_cli_process_identity_py___pid_alive_matches — traced copy of pi_pid_alive_matches (int) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern int pi_pid_alive_matches(long pid, int has_create, double create_time);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pi_pid_alive_matches <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pi_pid_alive_matches");
        int result = pi_pid_alive_matches(argv[i], argv[i], argv[i]);
            char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pi_pid_alive_matches");
    }
    return 0;
}
