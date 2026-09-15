/* gate: agent_lsp_workspace_py__is_inside_workspace — traced copy of lsw_is_inside_workspace */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lsw_is_inside_workspace(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("lsw_is_inside_workspace");
    int result = lsw_is_inside_workspace(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lsw_is_inside_workspace");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lsw_is_inside_workspace <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
