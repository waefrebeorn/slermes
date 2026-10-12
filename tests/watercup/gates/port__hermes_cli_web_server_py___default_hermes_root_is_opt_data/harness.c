/* gate: port__hermes_cli_web_server_py___default_hermes_root_is_opt_data — traced copy of default_hermes_root_is_opt_data (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool default_hermes_root_is_opt_data(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s default_hermes_root_is_opt_data <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("default_hermes_root_is_opt_data");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = default_hermes_root_is_opt_data();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("default_hermes_root_is_opt_data");
    }
    return 0;
}
