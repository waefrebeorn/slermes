/* gate: tools_skills_hub_py__install_from_quarantine — traced copy of skills_hub_install_from_source */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int skills_hub_install_from_source(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("skills_hub_install_from_source");
    int result = skills_hub_install_from_source(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("skills_hub_install_from_source");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skills_hub_install_from_source <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
