/* gate: hermes_cli_managed_uv_py___install_uv — traced copy of muv_u_install_uv */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int muv_u_install_uv(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("muv_u_install_uv");
    int result = muv_u_install_uv(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("muv_u_install_uv");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s muv_u_install_uv <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
