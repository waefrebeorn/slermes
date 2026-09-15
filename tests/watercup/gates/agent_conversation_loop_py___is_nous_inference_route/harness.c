/* gate: agent_conversation_loop_py___is_nous_inference_route — traced copy of cvl_is_nous_inference_route */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cvl_is_nous_inference_route(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cvl_is_nous_inference_route");
    int result = cvl_is_nous_inference_route(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cvl_is_nous_inference_route");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cvl_is_nous_inference_route <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
