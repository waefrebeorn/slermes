/* gate: gateway_cwd_placeholder_py__resolve_placeholder_terminal_cwd — traced copy of gateway_cwd_placeholder_resolve_placeholder_terminal_cwd */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_cwd_placeholder_resolve_placeholder_terminal_cwd(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_cwd_placeholder_resolve_placeholder_terminal_cwd");
    int result = gateway_cwd_placeholder_resolve_placeholder_terminal_cwd(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_cwd_placeholder_resolve_placeholder_terminal_cwd");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_cwd_placeholder_resolve_placeholder_terminal_cwd <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
