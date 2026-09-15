/* gate: tools_delegate_tool_py___model_background_value — traced copy of delegate_model_background_value */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int delegate_model_background_value(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("delegate_model_background_value");
    int result = delegate_model_background_value(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("delegate_model_background_value");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s delegate_model_background_value <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
