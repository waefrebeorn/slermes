/* gate: agent_skill_utils_py__skill_matches_platform — traced copy of sku_skill_matches_platform */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sku_skill_matches_platform(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sku_skill_matches_platform");
    int result = sku_skill_matches_platform(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sku_skill_matches_platform");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sku_skill_matches_platform <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
