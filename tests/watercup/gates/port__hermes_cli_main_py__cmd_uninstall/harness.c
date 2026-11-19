/* gate: port__hermes_cli_main_py__cmd_uninstall — traced copy of mn_cmd_uninstall (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int mn_cmd_uninstall(const char *args, bool gui);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mn_cmd_uninstall <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("mn_cmd_uninstall");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = mn_cmd_uninstall(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("mn_cmd_uninstall");
    }
    return 0;
}
