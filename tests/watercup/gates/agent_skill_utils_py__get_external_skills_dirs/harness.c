/* gate: agent_skill_utils_py__get_external_skills_dirs — traced copy of sku_get_external_skills_dirs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sku_get_external_skills_dirs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sku_get_external_skills_dirs");
    int result = sku_get_external_skills_dirs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sku_get_external_skills_dirs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sku_get_external_skills_dirs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
