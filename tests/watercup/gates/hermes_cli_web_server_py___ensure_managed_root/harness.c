/* gate: hermes_cli_web_server_py___ensure_managed_root — traced copy of ws_ensure_managed_root */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ws_ensure_managed_root(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ws_ensure_managed_root");
    int result = ws_ensure_managed_root(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_ensure_managed_root");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_ensure_managed_root <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
