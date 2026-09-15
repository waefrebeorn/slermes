/* gate: gateway_platforms_bluebubbles_py___find_registered_webhooks — traced copy of bb_u_find_registered_webhooks */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int bb_u_find_registered_webhooks(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("bb_u_find_registered_webhooks");
    int result = bb_u_find_registered_webhooks(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bb_u_find_registered_webhooks");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bb_u_find_registered_webhooks <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
