/* gate: tools_file_state_py__known_reads — traced copy of tools_file_state_known_reads */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_file_state_known_reads(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_file_state_known_reads");
    int result = tools_file_state_known_reads(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_file_state_known_reads");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_file_state_known_reads <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
