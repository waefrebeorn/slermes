/* gate: port__hermes_cli_profiles_py__profiles_to_serve — traced copy of profile_serve_next (bool_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool profile_serve_next(char *packed, char **cursor, const char **name,
                        const char **home);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profile_serve_next <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("profile_serve_next");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = profile_serve_next((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 2], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 2], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 2], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("profile_serve_next");
    }
    return 0;
}
