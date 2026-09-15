/* gate: tools_skill_manager_tool_py__apply_skill_pending — traced copy of smt_apply_skill_pending */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int smt_apply_skill_pending(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("smt_apply_skill_pending");
    int result = smt_apply_skill_pending(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("smt_apply_skill_pending");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s smt_apply_skill_pending <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
