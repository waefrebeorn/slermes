/* gate: real__hermes_cli_web_server_py___custom_endpoint_id — GAP sentinel for _custom_endpoint_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_custom_endpoint_id";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _custom_endpoint_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_custom_endpoint_id");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_custom_endpoint_id");
    }
    return 0;
}
