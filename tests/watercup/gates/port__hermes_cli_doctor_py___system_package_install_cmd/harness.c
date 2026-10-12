/* gate: port__hermes_cli_doctor_py___system_package_install_cmd — traced copy of doctor_system_package_install_cmd (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool doctor_system_package_install_cmd(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s doctor_system_package_install_cmd <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("doctor_system_package_install_cmd");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = doctor_system_package_install_cmd();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("doctor_system_package_install_cmd");
    }
    return 0;
}
