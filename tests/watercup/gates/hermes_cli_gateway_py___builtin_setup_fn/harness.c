/* gate: hermes_cli_gateway_py___builtin_setup_fn — traced copy of cgw_u_builtin_setup_fn */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_u_builtin_setup_fn(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_u_builtin_setup_fn");
    int result = cgw_u_builtin_setup_fn(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_u_builtin_setup_fn");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_u_builtin_setup_fn <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
