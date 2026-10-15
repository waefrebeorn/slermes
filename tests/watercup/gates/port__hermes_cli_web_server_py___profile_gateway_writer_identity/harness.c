/* gate: port__hermes_cli_web_server_py___profile_gateway_writer_identity — traced copy of _profile_gateway_writer_identity (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _profile_gateway_writer_identity(const char *profile);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _profile_gateway_writer_identity <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_profile_gateway_writer_identity");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _profile_gateway_writer_identity(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_profile_gateway_writer_identity");
    }
    return 0;
}
