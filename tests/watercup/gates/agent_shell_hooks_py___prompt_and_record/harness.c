/* gate: agent_shell_hooks_py___prompt_and_record — traced copy of shk_prompt_and_record */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int shk_prompt_and_record(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("shk_prompt_and_record");
    int result = shk_prompt_and_record(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("shk_prompt_and_record");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s shk_prompt_and_record <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
