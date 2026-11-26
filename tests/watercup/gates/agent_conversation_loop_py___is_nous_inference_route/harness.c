/* gate: agent_conversation_loop_py___is_nous_inference_route — traced copy of cvl_is_nous_inference_route */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cvl_is_nous_inference_route(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("cvl_is_nous_inference_route");
    int result = cvl_is_nous_inference_route(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cvl_is_nous_inference_route");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cvl_is_nous_inference_route <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
