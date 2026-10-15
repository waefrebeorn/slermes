/* gate: port__gateway_run_py___async_profile_runtime_scope — traced copy of _async_profile_runtime_scope (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _async_profile_runtime_scope(const char *profile_home);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _async_profile_runtime_scope <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_async_profile_runtime_scope");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _async_profile_runtime_scope(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_async_profile_runtime_scope");
    }
    return 0;
}
