/* gate: hermes_cli_web_server_py___is_accepted_host — traced copy of ws_is_accepted_host */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ws_is_accepted_host(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ws_is_accepted_host");
    int result = ws_is_accepted_host(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_is_accepted_host");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_is_accepted_host <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
