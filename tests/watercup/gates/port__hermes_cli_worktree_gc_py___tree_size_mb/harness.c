/* gate: port__hermes_cli_worktree_gc_py___tree_size_mb — traced copy of wgc_tree_size_mb (int) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern int wgc_tree_size_mb(const char *path, int *has_size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wgc_tree_size_mb <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("wgc_tree_size_mb");
        int result = wgc_tree_size_mb(argv[i], argv[i]);
            char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("wgc_tree_size_mb");
    }
    return 0;
}
