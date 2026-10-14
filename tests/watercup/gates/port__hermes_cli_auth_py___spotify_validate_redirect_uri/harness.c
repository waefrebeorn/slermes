/* gate: port__hermes_cli_auth_py___spotify_validate_redirect_uri — traced copy of _spotify_validate_redirect_uri (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _spotify_validate_redirect_uri(const char *redirect_uri, char **host_out,
                                  int *port_out, char **path_out, char *err, size_t errsz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _spotify_validate_redirect_uri <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_spotify_validate_redirect_uri");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _spotify_validate_redirect_uri(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_spotify_validate_redirect_uri");
    }
    return 0;
}
