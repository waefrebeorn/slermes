/* gate: port__hermes_cli_main_py__cmd_dashboard — traced copy of mn_cmd_dashboard (int_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int mn_cmd_dashboard(const char *args, const char *token_file);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mn_cmd_dashboard <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("mn_cmd_dashboard");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = mn_cmd_dashboard(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("mn_cmd_dashboard");
    }
    return 0;
}
