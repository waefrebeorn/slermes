/* gate: hermes_cli_skills_hub_py___is_valid_installed_skill_name -> real is_valid_installed_skill_name (int 1/0). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern int hermes_cli_skills_hub_u_is_valid_installed_skill_name(const char *name);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <name>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("hermes_cli_skills_hub_u_is_valid_installed_skill_name");
        fflush(stdout);
        int r = hermes_cli_skills_hub_u_is_valid_installed_skill_name(argv[i]);
        WCUP_VALINT(r);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("hermes_cli_skills_hub_u_is_valid_installed_skill_name");
    }
    return 0;
}
