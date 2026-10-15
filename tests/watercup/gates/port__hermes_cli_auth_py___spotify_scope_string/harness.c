/* gate: port__hermes_cli_auth_py___spotify_scope_string — traced copy of spotify_scope_string (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * spotify_scope_string(const char *raw_scope);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s spotify_scope_string <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("spotify_scope_string");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = spotify_scope_string(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("spotify_scope_string");
    }
    return 0;
}
