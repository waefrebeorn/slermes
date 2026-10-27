/* gate: port__hermes_cli_update_cmd_py___reload_config_modules — traced copy of update_reload_config_modules (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void update_reload_config_modules(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s update_reload_config_modules <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("update_reload_config_modules");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    update_reload_config_modules();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("update_reload_config_modules");
    }
    return 0;
}
