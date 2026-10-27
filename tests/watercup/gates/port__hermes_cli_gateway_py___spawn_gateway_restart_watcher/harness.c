/* gate: port__hermes_cli_gateway_py___spawn_gateway_restart_watcher — traced copy of gwc2_spawn_gateway_restart_watcher (int_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int gwc2_spawn_gateway_restart_watcher(const char *old_pid, const char *run_argv);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gwc2_spawn_gateway_restart_watcher <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("gwc2_spawn_gateway_restart_watcher");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = gwc2_spawn_gateway_restart_watcher(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("gwc2_spawn_gateway_restart_watcher");
    }
    return 0;
}
