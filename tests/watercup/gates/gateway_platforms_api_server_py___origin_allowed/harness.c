/* gate: gateway_platforms_api_server_py___origin_allowed — traced copy of aps_origin_allowed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aps_origin_allowed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("aps_origin_allowed");
    int result = aps_origin_allowed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aps_origin_allowed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aps_origin_allowed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
