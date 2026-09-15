/* gate: agent_tool_result_classification_py__file_mutation_result_landed — traced copy of trc_file_mutation_result_landed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int trc_file_mutation_result_landed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("trc_file_mutation_result_landed");
    int result = trc_file_mutation_result_landed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("trc_file_mutation_result_landed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s trc_file_mutation_result_landed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
