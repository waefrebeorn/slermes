/* gate: port__hermes_cli_main_py___electron_dist_ok — traced copy of _electron_dist_ok (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool _electron_dist_ok(const char* project_root);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _electron_dist_ok <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_electron_dist_ok");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = _electron_dist_ok(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_electron_dist_ok");
    }
    return 0;
}
