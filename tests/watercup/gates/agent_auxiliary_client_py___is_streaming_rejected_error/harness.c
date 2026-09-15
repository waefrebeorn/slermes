/* gate: agent_auxiliary_client_py___is_streaming_rejected_error — traced copy of aux__is_streaming_rejected_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__is_streaming_rejected_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("aux__is_streaming_rejected_error");
    int result = aux__is_streaming_rejected_error(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__is_streaming_rejected_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__is_streaming_rejected_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
