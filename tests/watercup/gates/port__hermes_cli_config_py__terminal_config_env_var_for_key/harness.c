/* gate: port__hermes_cli_config_py__terminal_config_env_var_for_key — traced copy of terminal_config_env_var_for_key (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void terminal_config_env_var_for_key(const char *key, char *out, size_t out_size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s terminal_config_env_var_for_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("terminal_config_env_var_for_key");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    terminal_config_env_var_for_key(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("terminal_config_env_var_for_key");
    }
    return 0;
}
