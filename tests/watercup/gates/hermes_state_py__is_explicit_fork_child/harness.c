/* gate: hermes_state_py__is_explicit_fork_child — traced copy of is_explicit_fork_child */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_explicit_fork_child(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_explicit_fork_child");
    bool result = is_explicit_fork_child(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_explicit_fork_child");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_explicit_fork_child <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
