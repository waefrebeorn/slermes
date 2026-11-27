/* gate: port__hermes_cli_checkpoints_py__cmd_clear_legacy — traced copy of cmd_clear_legacy (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void cmd_clear_legacy(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cmd_clear_legacy <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cmd_clear_legacy");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cmd_clear_legacy();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cmd_clear_legacy");
    }
    return 0;
}
