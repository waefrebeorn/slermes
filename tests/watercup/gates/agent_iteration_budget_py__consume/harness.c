/* gate: agent_iteration_budget_py__consume — traced copy of itb_consume */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool itb_consume(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("itb_consume");
    bool result = itb_consume(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("itb_consume");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s itb_consume <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
