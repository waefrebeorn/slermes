/* gate: port__hermes_cli_terminal_breadcrumbs_py___prune_stale — traced copy of tbr_prune_stale (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void tbr_prune_stale(const char *dir, double now);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tbr_prune_stale <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tbr_prune_stale");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    tbr_prune_stale();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tbr_prune_stale");
    }
    return 0;
}
