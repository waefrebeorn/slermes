/* gate: tools_lazy_deps_py__is_available — traced copy of tools_lazy_deps_available */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tools_lazy_deps_available(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_lazy_deps_available");
    bool result = tools_lazy_deps_available(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_lazy_deps_available");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_lazy_deps_available <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
