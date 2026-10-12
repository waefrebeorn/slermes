/* gate: port__hermes_cli_auth_py___file_lock — traced copy of auth_file_lock_release (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void auth_file_lock_release(const char *lock_path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_file_lock_release <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_file_lock_release");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    auth_file_lock_release(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_file_lock_release");
    }
    return 0;
}
