/* gate: agent_context_compressor_py__update_from_response — traced copy of cc_update_from_response */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cc_update_from_response(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cc_update_from_response");
    int result = cc_update_from_response(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cc_update_from_response");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cc_update_from_response <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
