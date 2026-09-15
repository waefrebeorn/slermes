/* gate: gateway_platforms_base_py__should_bypass_proxy — traced copy of pb_should_bypass_proxy */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pb_should_bypass_proxy(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pb_should_bypass_proxy");
    int result = pb_should_bypass_proxy(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pb_should_bypass_proxy");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pb_should_bypass_proxy <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
