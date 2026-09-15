/* gate: agent_trace_upload_py___content_to_blocks — traced copy of agent_trace_upload_u_content_to_blocks */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_trace_upload_u_content_to_blocks(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_trace_upload_u_content_to_blocks");
    int result = agent_trace_upload_u_content_to_blocks(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_trace_upload_u_content_to_blocks");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_trace_upload_u_content_to_blocks <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
