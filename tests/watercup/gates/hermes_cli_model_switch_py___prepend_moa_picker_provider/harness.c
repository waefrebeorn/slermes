/* gate: hermes_cli_model_switch_py___prepend_moa_picker_provider — traced copy of msw_u_prepend_moa_picker_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msw_u_prepend_moa_picker_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msw_u_prepend_moa_picker_provider");
    int result = msw_u_prepend_moa_picker_provider(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msw_u_prepend_moa_picker_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msw_u_prepend_moa_picker_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
