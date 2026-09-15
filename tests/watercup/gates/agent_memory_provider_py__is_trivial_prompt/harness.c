/* gate: agent_memory_provider_py__is_trivial_prompt — traced copy of is_trivial_prompt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int is_trivial_prompt(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_trivial_prompt");
    int result = is_trivial_prompt(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_trivial_prompt");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_trivial_prompt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
