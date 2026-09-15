/* gate: agent_model_metadata_py___is_connect_timeout — traced copy of mm_is_connect_timeout */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mm_is_connect_timeout(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mm_is_connect_timeout");
    bool result = mm_is_connect_timeout(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mm_is_connect_timeout");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mm_is_connect_timeout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
