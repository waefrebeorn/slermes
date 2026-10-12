/* gate: port__hermes_cli_auth_py___auth_store_lock — traced copy of authstore_unlock (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void authstore_unlock(const char *target_path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s authstore_unlock <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("authstore_unlock");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    authstore_unlock(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("authstore_unlock");
    }
    return 0;
}
