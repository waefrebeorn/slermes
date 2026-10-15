/* gate: port__tools_blueprints_py__blueprint_spec_for_installed — traced copy of cli_tools_blueprints_blueprint_spec_for_installed (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_tools_blueprints_blueprint_spec_for_installed(const char *skill_name, const char *skills_dir,
    char *schedule_out, size_t sched_size, int *found_out);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_blueprints_blueprint_spec_for_installed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_blueprints_blueprint_spec_for_installed");
    char _s2[256]; _s2[0] = 0;
    int _o4 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_tools_blueprints_blueprint_spec_for_installed(argv[i], argv[2 + (i - 2 + 1) % 5], _s2, sizeof(_s2), &_o4);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_s2);
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o4); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_blueprints_blueprint_spec_for_installed");
    }
    return 0;
}
