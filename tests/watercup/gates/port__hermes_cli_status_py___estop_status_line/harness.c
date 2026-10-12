/* gate: port__hermes_cli_status_py___estop_status_line — traced copy of estop_paused_reply (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void estop_paused_reply(const char *value, char out[64]);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s estop_paused_reply <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("estop_paused_reply");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    estop_paused_reply(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("estop_paused_reply");
    }
    return 0;
}
