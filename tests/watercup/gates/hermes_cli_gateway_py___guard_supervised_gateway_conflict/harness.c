/* gate: hermes_cli_gateway_py___guard_supervised_gateway_conflict — traced copy of cgw_u_guard_supervised_gateway_conflict */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_u_guard_supervised_gateway_conflict(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_u_guard_supervised_gateway_conflict");
    int result = cgw_u_guard_supervised_gateway_conflict(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_u_guard_supervised_gateway_conflict");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_u_guard_supervised_gateway_conflict <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
