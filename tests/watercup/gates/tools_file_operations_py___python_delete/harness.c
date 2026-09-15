/* gate: tools_file_operations_py___python_delete — traced copy of fo_python_delete */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int fo_python_delete(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("fo_python_delete");
    int result = fo_python_delete(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fo_python_delete");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_python_delete <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
