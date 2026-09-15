/* gate: agent_context_engine_py__update_model — traced copy of ceg_update_model */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ceg_update_model(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ceg_update_model");
    int result = ceg_update_model(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ceg_update_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ceg_update_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
