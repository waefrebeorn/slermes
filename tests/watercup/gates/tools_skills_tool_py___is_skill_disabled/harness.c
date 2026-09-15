/* gate: tools_skills_tool_py___is_skill_disabled — traced copy of skills_tool_is_skill_disabled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int skills_tool_is_skill_disabled(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("skills_tool_is_skill_disabled");
    int result = skills_tool_is_skill_disabled(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("skills_tool_is_skill_disabled");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skills_tool_is_skill_disabled <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
