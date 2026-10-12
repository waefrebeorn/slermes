/* gate: port__hermes_cli_goals_py___warn_dropped_write — traced copy of goal_warn_dropped_write (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void goal_warn_dropped_write(const char *manager, const char *kind, const char *session_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s goal_warn_dropped_write <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("goal_warn_dropped_write");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    goal_warn_dropped_write(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("goal_warn_dropped_write");
    }
    return 0;
}
