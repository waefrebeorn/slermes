/* gate: agent_anthropic_adapter_py___prefer_refreshable_claude_code_token — traced copy of antr_prefer_refreshable_claude_code_token */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int antr_prefer_refreshable_claude_code_token(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("antr_prefer_refreshable_claude_code_token");
    int result = antr_prefer_refreshable_claude_code_token(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("antr_prefer_refreshable_claude_code_token");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s antr_prefer_refreshable_claude_code_token <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
