/* gate: port__agent_prompt_builder_py__clear_skills_system_prompt_cache — traced copy of clear_skills_system_prompt_cache (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void clear_skills_system_prompt_cache(const char *hermes_home, bool clear_snapshot);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s clear_skills_system_prompt_cache <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("clear_skills_system_prompt_cache");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    clear_skills_system_prompt_cache(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("clear_skills_system_prompt_cache");
    }
    return 0;
}
