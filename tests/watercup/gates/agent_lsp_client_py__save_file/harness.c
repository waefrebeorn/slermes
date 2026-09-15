/* gate: agent_lsp_client_py__save_file — traced copy of lspc_save_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lspc_save_file(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("lspc_save_file");
    int result = lspc_save_file(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lspc_save_file");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lspc_save_file <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
