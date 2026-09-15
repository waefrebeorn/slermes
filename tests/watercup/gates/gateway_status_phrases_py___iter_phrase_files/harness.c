/* gate: gateway_status_phrases_py___iter_phrase_files — traced copy of gateway_status_phrases_u_iter_phrase_files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_status_phrases_u_iter_phrase_files(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_status_phrases_u_iter_phrase_files");
    int result = gateway_status_phrases_u_iter_phrase_files(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_status_phrases_u_iter_phrase_files");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_status_phrases_u_iter_phrase_files <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
