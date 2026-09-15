/* gate: tools_binary_extensions_py__has_binary_extension — traced copy of tools_binary_extensions_has_binary_extension */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_binary_extensions_has_binary_extension(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_binary_extensions_has_binary_extension");
    int result = tools_binary_extensions_has_binary_extension(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_binary_extensions_has_binary_extension");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_binary_extensions_has_binary_extension <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
