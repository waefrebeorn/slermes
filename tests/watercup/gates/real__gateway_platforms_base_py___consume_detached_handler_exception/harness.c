/* gate: real__gateway_platforms_base_py___consume_detached_handler_exception — GAP sentinel for _consume_detached_handler_exception */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_consume_detached_handler_exception";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _consume_detached_handler_exception <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_consume_detached_handler_exception");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_consume_detached_handler_exception");
    }
    return 0;
}
