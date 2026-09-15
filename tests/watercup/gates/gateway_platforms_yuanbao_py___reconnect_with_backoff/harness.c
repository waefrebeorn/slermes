/* gate: gateway_platforms_yuanbao_py___reconnect_with_backoff — traced copy of yb_u_reconnect_with_backoff */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_u_reconnect_with_backoff(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_u_reconnect_with_backoff");
    int result = yb_u_reconnect_with_backoff(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_u_reconnect_with_backoff");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_u_reconnect_with_backoff <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
