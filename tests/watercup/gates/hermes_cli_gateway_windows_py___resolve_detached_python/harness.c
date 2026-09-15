/* gate: hermes_cli_gateway_windows_py___resolve_detached_python — traced copy of gw_u_resolve_detached_python */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_u_resolve_detached_python(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_u_resolve_detached_python");
    int result = gw_u_resolve_detached_python(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_u_resolve_detached_python");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_u_resolve_detached_python <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
