/* gate: port__hermes_cli_update_cmd_py___no_prompt_git_kwargs — traced copy of _no_prompt_git_kwargs (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _no_prompt_git_kwargs(int assume_yes, int gateway_mode, const char *pre_update_snapshot_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _no_prompt_git_kwargs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_no_prompt_git_kwargs");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _no_prompt_git_kwargs(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_no_prompt_git_kwargs");
    }
    return 0;
}
