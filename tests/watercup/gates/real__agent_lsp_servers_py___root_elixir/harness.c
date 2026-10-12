/* gate: real__agent_lsp_servers_py___root_elixir — GAP sentinel for _root_elixir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_root_elixir";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _root_elixir <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_root_elixir");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_root_elixir");
    }
    return 0;
}
