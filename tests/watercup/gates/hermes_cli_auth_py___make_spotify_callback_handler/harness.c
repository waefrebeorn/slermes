/* gate: hermes_cli_auth_py___make_spotify_callback_handler — traced copy of auth_u_make_spotify_callback_handler */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auth_u_make_spotify_callback_handler(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auth_u_make_spotify_callback_handler");
    int result = auth_u_make_spotify_callback_handler(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auth_u_make_spotify_callback_handler");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_u_make_spotify_callback_handler <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
