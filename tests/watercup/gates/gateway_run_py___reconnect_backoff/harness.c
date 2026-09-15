/* gate: gateway_run_py___reconnect_backoff — traced copy of gw_reconnect_backoff */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_reconnect_backoff(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_reconnect_backoff");
    int result = gw_reconnect_backoff(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_reconnect_backoff");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_reconnect_backoff <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
