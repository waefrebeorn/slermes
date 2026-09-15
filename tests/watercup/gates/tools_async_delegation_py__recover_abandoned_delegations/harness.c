/* gate: tools_async_delegation_py__recover_abandoned_delegations — traced copy of adel_recover_abandoned_delegations */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int adel_recover_abandoned_delegations(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("adel_recover_abandoned_delegations");
    int result = adel_recover_abandoned_delegations(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("adel_recover_abandoned_delegations");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s adel_recover_abandoned_delegations <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
