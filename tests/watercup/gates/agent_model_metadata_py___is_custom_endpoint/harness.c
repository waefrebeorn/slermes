/* gate: agent_model_metadata_py___is_custom_endpoint — traced copy of is_custom_endpoint */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_custom_endpoint(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_custom_endpoint");
    bool result = is_custom_endpoint(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_custom_endpoint");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_custom_endpoint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
