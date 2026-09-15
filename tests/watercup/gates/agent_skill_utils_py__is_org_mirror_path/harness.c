/* gate: agent_skill_utils_py__is_org_mirror_path — traced copy of skillu_is_org_mirror_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int skillu_is_org_mirror_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("skillu_is_org_mirror_path");
    int result = skillu_is_org_mirror_path(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("skillu_is_org_mirror_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skillu_is_org_mirror_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
