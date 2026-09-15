/* gate: hermes_cli_azure_detect_py___looks_like_anthropic_path — traced copy of azure_looks_like_anthropic_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int azure_looks_like_anthropic_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("azure_looks_like_anthropic_path");
    int result = azure_looks_like_anthropic_path(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("azure_looks_like_anthropic_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s azure_looks_like_anthropic_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
