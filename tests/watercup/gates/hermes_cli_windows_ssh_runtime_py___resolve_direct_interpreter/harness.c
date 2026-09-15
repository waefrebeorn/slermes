/* gate: hermes_cli_windows_ssh_runtime_py___resolve_direct_interpreter — traced copy of wssr_u_resolve_direct_interpreter */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wssr_u_resolve_direct_interpreter(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wssr_u_resolve_direct_interpreter");
    int result = wssr_u_resolve_direct_interpreter(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wssr_u_resolve_direct_interpreter");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wssr_u_resolve_direct_interpreter <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
