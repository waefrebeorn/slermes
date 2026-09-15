/* gate: agent_lsp_cli_py__register_subparser — traced copy of lspc2_register_subparser */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lspc2_register_subparser(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("lspc2_register_subparser");
    int result = lspc2_register_subparser(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lspc2_register_subparser");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lspc2_register_subparser <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
