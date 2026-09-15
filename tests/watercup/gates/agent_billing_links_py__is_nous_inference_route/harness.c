/* gate: agent_billing_links_py__is_nous_inference_route — traced copy of is_nous_inference_route */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int is_nous_inference_route(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_nous_inference_route");
    int result = is_nous_inference_route(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_nous_inference_route");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_nous_inference_route <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
