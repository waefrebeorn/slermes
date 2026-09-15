/* gate: agent_iteration_budget_py__refund — traced copy of itb_refund */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int itb_refund(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("itb_refund");
    int result = itb_refund(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("itb_refund");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s itb_refund <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
