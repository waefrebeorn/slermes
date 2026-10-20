/* gate: port__hermes_cli_auth_py___spotify_build_authorize_url — traced copy of _spotify_build_authorize_url (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _spotify_build_authorize_url(const char *client_id, const char *redirect_uri,
                                  const char *scope, const char *state,
                                  const char *code_challenge, const char *accounts_base_url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _spotify_build_authorize_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_spotify_build_authorize_url");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _spotify_build_authorize_url(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5], argv[2 + (i - 2 + 5) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_spotify_build_authorize_url");
    }
    return 0;
}
