/* gate: tools_async_delegation_py__claim_completion_delivery — traced copy of adel_claim_completion_delivery */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int adel_claim_completion_delivery(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("adel_claim_completion_delivery");
    int result = adel_claim_completion_delivery(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("adel_claim_completion_delivery");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s adel_claim_completion_delivery <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
