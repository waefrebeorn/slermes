/* gate: real__gateway_platforms_api_server_py___parse_nonnegative_int — GAP sentinel for _parse_nonnegative_int */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_parse_nonnegative_int";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _parse_nonnegative_int <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_parse_nonnegative_int");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_parse_nonnegative_int");
    }
    return 0;
}
