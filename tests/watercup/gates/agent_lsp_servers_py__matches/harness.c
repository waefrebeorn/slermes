/* gate: agent_lsp_servers_py__matches — traced copy of lsp_server_matches */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lsp_server_matches(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("lsp_server_matches");
    int result = lsp_server_matches(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lsp_server_matches");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lsp_server_matches <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
