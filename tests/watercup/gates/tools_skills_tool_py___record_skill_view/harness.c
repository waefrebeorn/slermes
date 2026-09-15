/* gate: tools_skills_tool_py___record_skill_view — traced copy of skillstool_record_skill_view */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int skillstool_record_skill_view(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("skillstool_record_skill_view");
    int result = skillstool_record_skill_view(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("skillstool_record_skill_view");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skillstool_record_skill_view <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
