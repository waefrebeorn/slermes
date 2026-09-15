/* gate: hermes_cli_skills_hub_py__do_diff — traced copy of hermes_cli_skills_hub_do_diff */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_skills_hub_do_diff(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_skills_hub_do_diff");
    int result = hermes_cli_skills_hub_do_diff(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_skills_hub_do_diff");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_skills_hub_do_diff <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
