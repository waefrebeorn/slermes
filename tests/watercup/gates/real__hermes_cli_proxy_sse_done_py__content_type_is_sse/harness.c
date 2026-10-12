/* gate: real__hermes_cli_proxy_sse_done_py__content_type_is_sse — GAP sentinel for content_type_is_sse */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "content_type_is_sse";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s content_type_is_sse <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("content_type_is_sse");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("content_type_is_sse");
    }
    return 0;
}
