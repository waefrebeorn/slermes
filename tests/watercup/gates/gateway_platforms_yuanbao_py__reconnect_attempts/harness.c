/* gate: gateway_platforms_yuanbao_py__reconnect_attempts — traced copy of yb_reconnect_attempts */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_reconnect_attempts(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_reconnect_attempts");
    int result = yb_reconnect_attempts(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_reconnect_attempts");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_reconnect_attempts <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
