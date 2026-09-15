/* gate: agent_stream_single_writer_py__claim_stream_writer — traced copy of agent_stream_single_writer_claim_stream_writer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_stream_single_writer_claim_stream_writer(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_stream_single_writer_claim_stream_writer");
    int result = agent_stream_single_writer_claim_stream_writer(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_stream_single_writer_claim_stream_writer");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_stream_single_writer_claim_stream_writer <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
