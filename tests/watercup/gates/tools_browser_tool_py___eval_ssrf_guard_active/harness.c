/* gate: tools_browser_tool_py___eval_ssrf_guard_active — traced copy of browser_eval_ssrf_guard_active */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool browser_eval_ssrf_guard_active(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("browser_eval_ssrf_guard_active");
    bool result = browser_eval_ssrf_guard_active(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("browser_eval_ssrf_guard_active");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser_eval_ssrf_guard_active <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
