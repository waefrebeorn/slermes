/* gate: agent_redact_py___has_known_prefix_substring — traced copy of red_has_known_prefix_substring */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool red_has_known_prefix_substring(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("red_has_known_prefix_substring");
    bool result = red_has_known_prefix_substring(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("red_has_known_prefix_substring");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s red_has_known_prefix_substring <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
