/* gate: tools_skills_tool_py__skill_matches_environment — traced copy of skills_tool_skill_matches_environment */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int skills_tool_skill_matches_environment(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("skills_tool_skill_matches_environment");
    int result = skills_tool_skill_matches_environment(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("skills_tool_skill_matches_environment");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skills_tool_skill_matches_environment <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
