/* gate: tools_file_operations_py___zero_match_probe — traced copy of fo_zero_match_probe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int fo_zero_match_probe(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("fo_zero_match_probe");
    int result = fo_zero_match_probe(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fo_zero_match_probe");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_zero_match_probe <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
