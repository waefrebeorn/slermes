/* gate: port__hermes_cli_toolset_scope_py__toolset_allowed_for_platform — traced copy of toolset_allowed_for_platform (bool_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool toolset_allowed_for_platform(const char *ts_key, const char *platform);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s toolset_allowed_for_platform <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("toolset_allowed_for_platform");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = toolset_allowed_for_platform(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("toolset_allowed_for_platform");
    }
    return 0;
}
