/* gate: port__hermes_cli_update_receipt_py___prune_old_receipts — traced copy of prune_old_receipts (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void prune_old_receipts(const char *directory);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s prune_old_receipts <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("prune_old_receipts");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    prune_old_receipts();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("prune_old_receipts");
    }
    return 0;
}
