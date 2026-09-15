/* gate: gateway_status_phrases_py___relative_path_under — traced copy of gateway_status_phrases_u_relative_path_under */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_status_phrases_u_relative_path_under(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_status_phrases_u_relative_path_under");
    int result = gateway_status_phrases_u_relative_path_under(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_status_phrases_u_relative_path_under");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_status_phrases_u_relative_path_under <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
