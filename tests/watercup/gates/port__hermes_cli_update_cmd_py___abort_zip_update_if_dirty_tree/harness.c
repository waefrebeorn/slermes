/* gate: port__hermes_cli_update_cmd_py___abort_zip_update_if_dirty_tree — traced copy of _abort_zip_update_if_dirty_tree (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _abort_zip_update_if_dirty_tree(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _abort_zip_update_if_dirty_tree <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_abort_zip_update_if_dirty_tree");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _abort_zip_update_if_dirty_tree(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_abort_zip_update_if_dirty_tree");
    }
    return 0;
}
