/* gate: port__gateway_pairing_py___sync_allowlist_add — traced copy of sync_allowlist_add (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void sync_allowlist_add(const char *platform, const char *user_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sync_allowlist_add <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sync_allowlist_add");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    sync_allowlist_add(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sync_allowlist_add");
    }
    return 0;
}
