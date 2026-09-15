/* gate: tools_skills_hub_py___matches — traced copy of skills_hub_matches */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int skills_hub_matches(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("skills_hub_matches");
    int result = skills_hub_matches(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("skills_hub_matches");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skills_hub_matches <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
