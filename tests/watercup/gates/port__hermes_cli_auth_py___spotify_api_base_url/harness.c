/* gate: port__hermes_cli_auth_py___spotify_api_base_url — traced copy of spotify_api_base_url (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * spotify_api_base_url(const char *state_api_base_url, const char *default_url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s spotify_api_base_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("spotify_api_base_url");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = spotify_api_base_url(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("spotify_api_base_url");
    }
    return 0;
}
