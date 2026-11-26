/* gate: port__hermes_cli_config_py__apply_terminal_config_to_env — traced copy of cfg_apply_terminal_config_to_env (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cfg_apply_terminal_config_to_env(const char *a0);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cfg_apply_terminal_config_to_env <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cfg_apply_terminal_config_to_env");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cfg_apply_terminal_config_to_env();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cfg_apply_terminal_config_to_env");
    }
    return 0;
}
