/* gate: port__hermes_cli_profiles_py___migrate_honcho_profile_host — traced copy of profile_migrate_honcho_profile_host (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void profile_migrate_honcho_profile_host(const char *old_name,
                                          const char *new_name,
                                          const char *new_dir);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_migrate_honcho_profile_host <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("profile_migrate_honcho_profile_host");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    profile_migrate_honcho_profile_host(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("profile_migrate_honcho_profile_host");
    }
    return 0;
}
