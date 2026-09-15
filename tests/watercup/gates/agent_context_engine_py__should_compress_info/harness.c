/* gate: agent_context_engine_py__should_compress_info — traced copy of agent_context_engine_should_compress_info */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_context_engine_should_compress_info(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_context_engine_should_compress_info");
    int result = agent_context_engine_should_compress_info(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_context_engine_should_compress_info");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_context_engine_should_compress_info <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
