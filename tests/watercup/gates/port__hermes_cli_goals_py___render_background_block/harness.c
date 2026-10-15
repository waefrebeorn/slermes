/* gate: port__hermes_cli_goals_py___render_background_block — traced copy of goal_judge_render_background_block (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * goal_judge_render_background_block(const char *const *json_processes);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s goal_judge_render_background_block <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("goal_judge_render_background_block");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = goal_judge_render_background_block(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("goal_judge_render_background_block");
    }
    return 0;
}
