/* gate: agent_auxiliary_client_py___is_unsupported_parameter_error — traced copy of auxc_is_unsupported_parameter_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auxc_is_unsupported_parameter_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auxc_is_unsupported_parameter_error");
    int result = auxc_is_unsupported_parameter_error(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auxc_is_unsupported_parameter_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auxc_is_unsupported_parameter_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
