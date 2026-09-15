/* gate: agent_auxiliary_client_py___strict_vision_backend_available — traced copy of auxc_strict_vision_backend_available */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool auxc_strict_vision_backend_available(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auxc_strict_vision_backend_available");
    bool result = auxc_strict_vision_backend_available(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auxc_strict_vision_backend_available");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auxc_strict_vision_backend_available <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
