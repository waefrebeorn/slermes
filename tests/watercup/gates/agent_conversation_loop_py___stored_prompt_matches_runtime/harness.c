/* gate: agent_conversation_loop_py___stored_prompt_matches_runtime — traced copy of cvl_stored_prompt_matches_runtime */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cvl_stored_prompt_matches_runtime(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cvl_stored_prompt_matches_runtime");
    int result = cvl_stored_prompt_matches_runtime(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cvl_stored_prompt_matches_runtime");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cvl_stored_prompt_matches_runtime <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
