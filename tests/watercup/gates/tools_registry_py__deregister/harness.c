/* gate: tools_registry_py__deregister — traced copy of deregister */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool deregister(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("deregister");
    bool result = deregister(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("deregister");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s deregister <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
