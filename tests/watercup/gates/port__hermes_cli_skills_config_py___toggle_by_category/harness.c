/* gate: port__hermes_cli_skills_config_py___toggle_by_category — traced copy of cli_hermes_cli_skills_config__toggle_by_category (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_hermes_cli_skills_config__toggle_by_category(const char *skill_names[], int skill_count,
    const char *disabled[], int disabled_count,
    char *new_disabled[], int *new_disabled_count, int max_disabled);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_hermes_cli_skills_config__toggle_by_category <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_hermes_cli_skills_config__toggle_by_category");
    int _o5 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_hermes_cli_skills_config__toggle_by_category(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), argv[2 + (i - 2 + 2) % 5], (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), argv[2 + (i - 2 + 4) % 5], &_o5, (int)strtol(argv[2 + (i - 2 + 6) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o5); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_hermes_cli_skills_config__toggle_by_category");
    }
    return 0;
}
