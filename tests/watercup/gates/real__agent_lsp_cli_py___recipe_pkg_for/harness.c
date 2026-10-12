/* gate: real__agent_lsp_cli_py___recipe_pkg_for — GAP sentinel for _recipe_pkg_for */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_recipe_pkg_for";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _recipe_pkg_for <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_recipe_pkg_for");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_recipe_pkg_for");
    }
    return 0;
}
