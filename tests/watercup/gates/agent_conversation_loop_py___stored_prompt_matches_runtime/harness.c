/* gate: agent_conversation_loop_py___stored_prompt_matches_runtime — traced copy of cvl_stored_prompt_matches_runtime */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cvl_stored_prompt_matches_runtime(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("cvl_stored_prompt_matches_runtime");
    int result = cvl_stored_prompt_matches_runtime(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cvl_stored_prompt_matches_runtime");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cvl_stored_prompt_matches_runtime <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
