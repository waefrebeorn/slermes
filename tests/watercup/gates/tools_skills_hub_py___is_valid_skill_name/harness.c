/* gate: tools_skills_hub_py___is_valid_skill_name — traced copy of skills_hub_is_valid_skill_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool skills_hub_is_valid_skill_name(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("skills_hub_is_valid_skill_name");
    bool result = skills_hub_is_valid_skill_name(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("skills_hub_is_valid_skill_name");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skills_hub_is_valid_skill_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
