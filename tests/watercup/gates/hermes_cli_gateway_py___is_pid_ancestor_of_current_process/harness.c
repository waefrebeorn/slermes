/* gate: hermes_cli_gateway_py___is_pid_ancestor_of_current_process — traced copy of is_pid_ancestor_of_current_process */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int is_pid_ancestor_of_current_process(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_pid_ancestor_of_current_process");
    int result = is_pid_ancestor_of_current_process(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_pid_ancestor_of_current_process");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_pid_ancestor_of_current_process <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
