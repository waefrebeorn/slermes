/* gate: tools_browser_tool_py__check_browser_routed_requirements — traced copy of browser_check_browser_routed_requirements */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool browser_check_browser_routed_requirements(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("browser_check_browser_routed_requirements");
    bool result = browser_check_browser_routed_requirements(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("browser_check_browser_routed_requirements");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser_check_browser_routed_requirements <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
