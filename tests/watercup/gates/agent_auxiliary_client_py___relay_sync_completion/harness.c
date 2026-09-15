/* gate: agent_auxiliary_client_py___relay_sync_completion — traced copy of auxc_relay_sync_completion */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auxc_relay_sync_completion(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auxc_relay_sync_completion");
    int result = auxc_relay_sync_completion(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auxc_relay_sync_completion");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auxc_relay_sync_completion <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
