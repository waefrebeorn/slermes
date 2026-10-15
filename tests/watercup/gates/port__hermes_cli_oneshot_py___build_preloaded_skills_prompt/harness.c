/* gate: port__hermes_cli_oneshot_py___build_preloaded_skills_prompt — traced copy of _build_preloaded_skills_prompt (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _build_preloaded_skills_prompt(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _build_preloaded_skills_prompt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_build_preloaded_skills_prompt");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _build_preloaded_skills_prompt();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_build_preloaded_skills_prompt");
    }
    return 0;
}
