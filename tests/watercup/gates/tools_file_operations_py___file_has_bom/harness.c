/* gate: tools_file_operations_py___file_has_bom — traced copy of fo_file_has_bom */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int fo_file_has_bom(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("fo_file_has_bom");
    int result = fo_file_has_bom(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fo_file_has_bom");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_file_has_bom <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
