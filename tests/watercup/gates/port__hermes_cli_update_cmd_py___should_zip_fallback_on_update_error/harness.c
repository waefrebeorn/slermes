/* gate: port__hermes_cli_update_cmd_py___should_zip_fallback_on_update_error — traced copy of uc_should_zip_fallback_on_update_error (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool uc_should_zip_fallback_on_update_error(int exit_code, bool is_git_error, bool is_windows);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_should_zip_fallback_on_update_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_should_zip_fallback_on_update_error");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = uc_should_zip_fallback_on_update_error((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_should_zip_fallback_on_update_error");
    }
    return 0;
}
