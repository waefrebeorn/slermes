/* gate: port__hermes_cli_agent_plugins_py___valid_skill_frontmatter — traced copy of ap_valid_skill_frontmatter (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * ap_valid_skill_frontmatter(const char *name, const char *description);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ap_valid_skill_frontmatter <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ap_valid_skill_frontmatter");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = ap_valid_skill_frontmatter(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ap_valid_skill_frontmatter");
    }
    return 0;
}
