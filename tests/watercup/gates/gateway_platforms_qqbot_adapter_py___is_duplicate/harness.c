/* gate: gateway_platforms_qqbot_adapter_py___is_duplicate — traced copy of helpers_is_duplicate */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int helpers_is_duplicate(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("helpers_is_duplicate");
    int result = helpers_is_duplicate(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("helpers_is_duplicate");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s helpers_is_duplicate <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
