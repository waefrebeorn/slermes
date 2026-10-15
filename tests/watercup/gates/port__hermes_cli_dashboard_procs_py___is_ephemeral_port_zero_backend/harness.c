/* gate: port__hermes_cli_dashboard_procs_py___is_ephemeral_port_zero_backend — traced copy of dashboard_procs_is_ephemeral_port_zero_backend (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int dashboard_procs_is_ephemeral_port_zero_backend(const char *argv_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dashboard_procs_is_ephemeral_port_zero_backend <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("dashboard_procs_is_ephemeral_port_zero_backend");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = dashboard_procs_is_ephemeral_port_zero_backend(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("dashboard_procs_is_ephemeral_port_zero_backend");
    }
    return 0;
}
