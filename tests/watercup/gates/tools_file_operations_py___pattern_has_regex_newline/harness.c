/* gate: tools_file_operations_py___pattern_has_regex_newline — traced copy of file_ops_pattern_has_regex_newline */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool file_ops_pattern_has_regex_newline(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("file_ops_pattern_has_regex_newline");
    bool result = file_ops_pattern_has_regex_newline(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("file_ops_pattern_has_regex_newline");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_ops_pattern_has_regex_newline <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
