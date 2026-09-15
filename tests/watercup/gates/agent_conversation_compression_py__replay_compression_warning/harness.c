/* gate: agent_conversation_compression_py__replay_compression_warning — traced copy of ccp_replay_compression_warning */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ccp_replay_compression_warning(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ccp_replay_compression_warning");
    int result = ccp_replay_compression_warning(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ccp_replay_compression_warning");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ccp_replay_compression_warning <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
