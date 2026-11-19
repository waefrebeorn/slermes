/* gate: port__hermes_cli_auth_py___is_terminal_xai_oauth_refresh_error — traced copy of auth_is_terminal_xai_oauth_refresh_error (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool auth_is_terminal_xai_oauth_refresh_error(const char *provider, const char *code,
                                              bool relogin_required);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_is_terminal_xai_oauth_refresh_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_is_terminal_xai_oauth_refresh_error");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = auth_is_terminal_xai_oauth_refresh_error(argv[i], argv[2 + (i - 2 + 1) % 5], (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_is_terminal_xai_oauth_refresh_error");
    }
    return 0;
}
