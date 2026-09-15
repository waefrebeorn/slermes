/* gate: hermes_cli_auth_py__login_spotify_command — traced copy of auth_login_spotify_command */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auth_login_spotify_command(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auth_login_spotify_command");
    int result = auth_login_spotify_command(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auth_login_spotify_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_login_spotify_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
