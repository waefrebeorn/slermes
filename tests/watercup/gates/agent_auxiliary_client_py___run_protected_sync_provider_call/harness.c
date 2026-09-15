/* gate: agent_auxiliary_client_py___run_protected_sync_provider_call — traced copy of auxc_run_protected_sync_provider_call */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auxc_run_protected_sync_provider_call(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auxc_run_protected_sync_provider_call");
    int result = auxc_run_protected_sync_provider_call(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auxc_run_protected_sync_provider_call");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auxc_run_protected_sync_provider_call <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
