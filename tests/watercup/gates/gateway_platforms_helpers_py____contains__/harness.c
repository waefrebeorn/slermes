/* gate: gateway_platforms_helpers_py____contains__ — traced copy of helpers___contains__ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int helpers___contains__(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("helpers___contains__");
    int result = helpers___contains__(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("helpers___contains__");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s helpers___contains__ <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
