/* gate: port__hermes_cli_update_cmd_py___no_prompt_git_kwargs — traced copy of _no_prompt_git_kwargs (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _no_prompt_git_kwargs(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _no_prompt_git_kwargs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_no_prompt_git_kwargs");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _no_prompt_git_kwargs();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_no_prompt_git_kwargs");
    }
    return 0;
}
