/* gate: port__hermes_cli_profiles_py___maybe_register_gateway_service — traced copy of profile_maybe_register_gateway_service (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void profile_maybe_register_gateway_service(const char *profile_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_maybe_register_gateway_service <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("profile_maybe_register_gateway_service");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    profile_maybe_register_gateway_service(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("profile_maybe_register_gateway_service");
    }
    return 0;
}
