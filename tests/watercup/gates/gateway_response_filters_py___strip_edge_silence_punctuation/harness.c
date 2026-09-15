/* gate: gateway_response_filters_py___strip_edge_silence_punctuation — traced copy of gateway_response_filters_u_strip_edge_silence_punctuation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_response_filters_u_strip_edge_silence_punctuation(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_response_filters_u_strip_edge_silence_punctuation");
    int result = gateway_response_filters_u_strip_edge_silence_punctuation(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_response_filters_u_strip_edge_silence_punctuation");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_response_filters_u_strip_edge_silence_punctuation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
