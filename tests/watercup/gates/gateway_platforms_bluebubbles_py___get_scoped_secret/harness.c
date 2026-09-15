/* gate: gateway_platforms_bluebubbles_py___get_scoped_secret — traced copy of bb_u_get_scoped_secret */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int bb_u_get_scoped_secret(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("bb_u_get_scoped_secret");
    int result = bb_u_get_scoped_secret(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bb_u_get_scoped_secret");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bb_u_get_scoped_secret <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
