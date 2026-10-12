/* gate: port__hermes_cli_update_cmd_py___prune_orphan_rescue_refs — traced copy of _prune_orphan_rescue_refs (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _prune_orphan_rescue_refs(const char *git_cmd, const char *cwd, const char *branch, int keep, int max_age_days);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _prune_orphan_rescue_refs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_prune_orphan_rescue_refs");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _prune_orphan_rescue_refs(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_prune_orphan_rescue_refs");
    }
    return 0;
}
