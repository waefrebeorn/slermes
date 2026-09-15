/* gate: agent_model_metadata_py__is_local_endpoint — traced copy of is_local_endpoint */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_local_endpoint(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_local_endpoint");
    bool result = is_local_endpoint(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_local_endpoint");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_local_endpoint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
