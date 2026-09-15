/* gate: gateway_status_phrases_py___clean_phrase_list — traced copy of gateway_status_phrases_u_clean_phrase_list */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_status_phrases_u_clean_phrase_list(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_status_phrases_u_clean_phrase_list");
    int result = gateway_status_phrases_u_clean_phrase_list(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_status_phrases_u_clean_phrase_list");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_status_phrases_u_clean_phrase_list <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
