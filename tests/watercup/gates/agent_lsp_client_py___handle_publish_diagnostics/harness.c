/* gate: agent_lsp_client_py___handle_publish_diagnostics — traced copy of lspc_handle_publish_diagnostics */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lspc_handle_publish_diagnostics(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("lspc_handle_publish_diagnostics");
    int result = lspc_handle_publish_diagnostics(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lspc_handle_publish_diagnostics");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lspc_handle_publish_diagnostics <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
