/* gate: port__gateway_pairing_py___allowlist_env_for_platform — traced copy of pairing_allowlist_env_for_platform (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * pairing_allowlist_env_for_platform(const char *platform);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pairing_allowlist_env_for_platform <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pairing_allowlist_env_for_platform");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = pairing_allowlist_env_for_platform(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pairing_allowlist_env_for_platform");
    }
    return 0;
}
