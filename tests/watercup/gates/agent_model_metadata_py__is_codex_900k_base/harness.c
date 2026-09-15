/* gate: agent_model_metadata_py__is_codex_900k_base — traced copy of is_codex_900k_base */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_codex_900k_base(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_codex_900k_base");
    bool result = is_codex_900k_base(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_codex_900k_base");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_codex_900k_base <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
