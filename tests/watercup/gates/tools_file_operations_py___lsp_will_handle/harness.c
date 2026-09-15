/* gate: tools_file_operations_py___lsp_will_handle — traced copy of fo_lsp_will_handle */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool fo_lsp_will_handle(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("fo_lsp_will_handle");
    bool result = fo_lsp_will_handle(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fo_lsp_will_handle");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_lsp_will_handle <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
