/* gate: port__hermes_cli_setup_py__run_setup_action_with_navigation — traced copy of run_setup_action_with_navigation (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int run_setup_action_with_navigation(const char *action);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s run_setup_action_with_navigation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("run_setup_action_with_navigation");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = run_setup_action_with_navigation(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("run_setup_action_with_navigation");
    }
    return 0;
}
