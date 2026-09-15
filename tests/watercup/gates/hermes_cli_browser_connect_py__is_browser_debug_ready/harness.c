/* gate: hermes_cli_browser_connect_py__is_browser_debug_ready — traced copy of is_browser_debug_ready */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int is_browser_debug_ready(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_browser_debug_ready");
    int result = is_browser_debug_ready(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_browser_debug_ready");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_browser_debug_ready <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
