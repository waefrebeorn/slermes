/* gate: real__hermes_cli_web_server_py__host_header_middleware — GAP sentinel for host_header_middleware */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "host_header_middleware";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s host_header_middleware <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("host_header_middleware");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("host_header_middleware");
    }
    return 0;
}
