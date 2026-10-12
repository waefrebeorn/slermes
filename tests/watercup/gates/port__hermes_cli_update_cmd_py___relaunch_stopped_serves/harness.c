/* gate: port__hermes_cli_update_cmd_py___relaunch_stopped_serves — traced copy of _relaunch_stopped_serves (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _relaunch_stopped_serves(const char *token);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _relaunch_stopped_serves <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_relaunch_stopped_serves");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _relaunch_stopped_serves(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_relaunch_stopped_serves");
    }
    return 0;
}
