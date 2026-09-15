/* gate: gateway_relay_ws_transport_py__connect — traced copy of wst_connect */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wst_connect(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wst_connect");
    int result = wst_connect(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wst_connect");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wst_connect <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
