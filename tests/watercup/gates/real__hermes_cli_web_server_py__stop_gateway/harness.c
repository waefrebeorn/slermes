/* gate: real__hermes_cli_web_server_py__stop_gateway — GAP sentinel for stop_gateway */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "stop_gateway";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stop_gateway <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("stop_gateway");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("stop_gateway");
    }
    return 0;
}
