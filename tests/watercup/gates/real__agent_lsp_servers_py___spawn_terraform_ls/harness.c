/* gate: real__agent_lsp_servers_py___spawn_terraform_ls — GAP sentinel for _spawn_terraform_ls */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_spawn_terraform_ls";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _spawn_terraform_ls <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_spawn_terraform_ls");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_spawn_terraform_ls");
    }
    return 0;
}
