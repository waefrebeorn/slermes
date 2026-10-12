/* gate: port__hermes_cli_main_py___try_ultrafast_version — traced copy of main_g_try_ultrafast_version (bool_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool main_g_try_ultrafast_version(int argc, char **argv);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_g_try_ultrafast_version <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("main_g_try_ultrafast_version");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = main_g_try_ultrafast_version((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("main_g_try_ultrafast_version");
    }
    return 0;
}
