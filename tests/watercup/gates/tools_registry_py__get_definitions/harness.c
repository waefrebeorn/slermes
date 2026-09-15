/* gate: tools_registry_py__get_definitions — traced copy of tools_registry_get_definitions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_registry_get_definitions(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_registry_get_definitions");
    int result = tools_registry_get_definitions(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_registry_get_definitions");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_registry_get_definitions <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
