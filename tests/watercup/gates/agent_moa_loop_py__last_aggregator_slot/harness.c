/* gate: agent_moa_loop_py__last_aggregator_slot — traced copy of moa_last_aggregator_slot */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int moa_last_aggregator_slot(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("moa_last_aggregator_slot");
    int result = moa_last_aggregator_slot(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("moa_last_aggregator_slot");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s moa_last_aggregator_slot <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
