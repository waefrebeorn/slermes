/* gate: agent_bounded_response_py__read_streaming_error_body — traced copy of agent_bounded_response_read_streaming_error_body */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_bounded_response_read_streaming_error_body(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_bounded_response_read_streaming_error_body");
    int result = agent_bounded_response_read_streaming_error_body(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_bounded_response_read_streaming_error_body");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_bounded_response_read_streaming_error_body <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
