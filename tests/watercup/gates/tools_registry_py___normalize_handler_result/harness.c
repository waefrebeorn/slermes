/* gate: tools_registry_py___normalize_handler_result — traced copy of tools_registry_u_normalize_handler_result */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_registry_u_normalize_handler_result(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_registry_u_normalize_handler_result");
    int result = tools_registry_u_normalize_handler_result(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_registry_u_normalize_handler_result");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_registry_u_normalize_handler_result <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
