/* gate: agent_lsp_client_py___handle_register_capability — traced copy of lspc_handle_register_capability */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lspc_handle_register_capability(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("lspc_handle_register_capability");
    int result = lspc_handle_register_capability(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lspc_handle_register_capability");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lspc_handle_register_capability <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
