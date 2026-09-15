/* gate: agent_conversation_compression_py__start — traced copy of ccp_start */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ccp_start(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ccp_start");
    int result = ccp_start(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ccp_start");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ccp_start <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
