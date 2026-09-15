/* gate: tools_skill_manager_tool_py__mark_background_review_skill_read — traced copy of smt_mark_background_review_skill_read */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int smt_mark_background_review_skill_read(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("smt_mark_background_review_skill_read");
    int result = smt_mark_background_review_skill_read(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("smt_mark_background_review_skill_read");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s smt_mark_background_review_skill_read <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
