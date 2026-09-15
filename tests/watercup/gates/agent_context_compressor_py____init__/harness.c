/* gate: agent_context_compressor_py____init__ — traced copy of cc_init */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cc_init(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cc_init");
    int result = cc_init(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cc_init");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cc_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
