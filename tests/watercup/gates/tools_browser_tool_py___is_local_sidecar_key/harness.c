/* gate: tools_browser_tool_py___is_local_sidecar_key — traced copy of browser_is_local_sidecar_key */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool browser_is_local_sidecar_key(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("browser_is_local_sidecar_key");
    bool result = browser_is_local_sidecar_key(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("browser_is_local_sidecar_key");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser_is_local_sidecar_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
