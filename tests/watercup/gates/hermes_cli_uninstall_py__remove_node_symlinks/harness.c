/* gate: hermes_cli_uninstall_py__remove_node_symlinks — traced copy of uninst_remove_node_symlinks */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int uninst_remove_node_symlinks(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("uninst_remove_node_symlinks");
    int result = uninst_remove_node_symlinks(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uninst_remove_node_symlinks");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uninst_remove_node_symlinks <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
