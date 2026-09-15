/* gate: tools_file_operations_py___has_bom — traced copy of file_text_ops_has_bom */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool file_text_ops_has_bom(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("file_text_ops_has_bom");
    bool result = file_text_ops_has_bom(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("file_text_ops_has_bom");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_text_ops_has_bom <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
