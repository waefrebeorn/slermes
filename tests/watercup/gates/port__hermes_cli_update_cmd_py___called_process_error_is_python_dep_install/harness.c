/* gate: port__hermes_cli_update_cmd_py___called_process_error_is_python_dep_install — traced copy of upd_called_process_error_is_python_dep_install (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool upd_called_process_error_is_python_dep_install(char *const cmd[]);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s upd_called_process_error_is_python_dep_install <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("upd_called_process_error_is_python_dep_install");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = upd_called_process_error_is_python_dep_install(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("upd_called_process_error_is_python_dep_install");
    }
    return 0;
}
