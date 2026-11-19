/* gate: port__hermes_cli_config_py__warn_deprecated_cwd_env_vars — traced copy of cfg_warn_deprecated_cwd_env_vars (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void cfg_warn_deprecated_cwd_env_vars(const char *config_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cfg_warn_deprecated_cwd_env_vars <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cfg_warn_deprecated_cwd_env_vars");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cfg_warn_deprecated_cwd_env_vars();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cfg_warn_deprecated_cwd_env_vars");
    }
    return 0;
}
