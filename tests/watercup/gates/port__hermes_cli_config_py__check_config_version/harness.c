/* gate: port__hermes_cli_config_py__check_config_version — traced copy of config_check_config_version (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void config_check_config_version(int *current, int *latest);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s config_check_config_version <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("config_check_config_version");
    int _o0 = 0;
    int _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    config_check_config_version(&_o0, &_o1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o0); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o1); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("config_check_config_version");
    }
    return 0;
}
