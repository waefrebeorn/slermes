/* gate: port__hermes_cli_config_py___install_method_project_root — traced copy of cfg_install_method_project_root (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * cfg_install_method_project_root(const char *module_dir);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cfg_install_method_project_root <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cfg_install_method_project_root");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = cfg_install_method_project_root(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cfg_install_method_project_root");
    }
    return 0;
}
