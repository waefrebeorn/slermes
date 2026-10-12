/* gate: real__hermes_cli_uninstall_py__remove_node_symlinks — GAP sentinel for remove_node_symlinks */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "remove_node_symlinks";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s remove_node_symlinks <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("remove_node_symlinks");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("remove_node_symlinks");
    }
    return 0;
}
