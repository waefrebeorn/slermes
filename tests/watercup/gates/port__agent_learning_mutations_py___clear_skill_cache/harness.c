/* gate: port__agent_learning_mutations_py___clear_skill_cache — traced copy of learning_mutations_clear_skill_cache (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void learning_mutations_clear_skill_cache(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s learning_mutations_clear_skill_cache <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("learning_mutations_clear_skill_cache");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    learning_mutations_clear_skill_cache(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("learning_mutations_clear_skill_cache");
    }
    return 0;
}
