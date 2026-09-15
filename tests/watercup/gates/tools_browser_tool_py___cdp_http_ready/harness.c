/* gate: tools_browser_tool_py___cdp_http_ready — traced copy of _cdp_http_ready */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _cdp_http_ready(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_cdp_http_ready");
    int result = _cdp_http_ready(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_cdp_http_ready");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _cdp_http_ready <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
