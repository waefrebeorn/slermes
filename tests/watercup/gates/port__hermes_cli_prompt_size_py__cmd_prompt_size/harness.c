/* gate: port__hermes_cli_prompt_size_py__cmd_prompt_size — traced copy of cli_hermes_cli_prompt_size_cmd_prompt_size (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void cli_hermes_cli_prompt_size_cmd_prompt_size(const char *platform, int as_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_hermes_cli_prompt_size_cmd_prompt_size <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_hermes_cli_prompt_size_cmd_prompt_size");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cli_hermes_cli_prompt_size_cmd_prompt_size();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_hermes_cli_prompt_size_cmd_prompt_size");
    }
    return 0;
}
