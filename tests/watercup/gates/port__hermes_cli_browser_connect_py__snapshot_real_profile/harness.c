/* gate: port__hermes_cli_browser_connect_py__snapshot_real_profile — traced copy of snapshot_real_profile (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * snapshot_real_profile(const char *browser);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s snapshot_real_profile <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("snapshot_real_profile");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = snapshot_real_profile(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("snapshot_real_profile");
    }
    return 0;
}
