/* gate: port__hermes_cli_gateway_py__launch_detached_gateway_restart_by_cmdline — traced copy of gwc2_launch_detached_gateway_restart_by_cmdline (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int gwc2_launch_detached_gateway_restart_by_cmdline(const char *run_argv);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gwc2_launch_detached_gateway_restart_by_cmdline <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("gwc2_launch_detached_gateway_restart_by_cmdline");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = gwc2_launch_detached_gateway_restart_by_cmdline(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("gwc2_launch_detached_gateway_restart_by_cmdline");
    }
    return 0;
}
