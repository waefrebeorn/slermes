/* gate: agent_auxiliary_client_py___captured_aux_cancel_requested — traced copy of auxc_captured_aux_cancel_requested */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auxc_captured_aux_cancel_requested(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auxc_captured_aux_cancel_requested");
    int result = auxc_captured_aux_cancel_requested(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auxc_captured_aux_cancel_requested");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auxc_captured_aux_cancel_requested <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
