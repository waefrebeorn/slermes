/* gate: agent_auxiliary_client_py___relay_auxiliary_call_async — traced copy of auxc_relay_auxiliary_call_async */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auxc_relay_auxiliary_call_async(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auxc_relay_auxiliary_call_async");
    int result = auxc_relay_auxiliary_call_async(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auxc_relay_auxiliary_call_async");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auxc_relay_auxiliary_call_async <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
