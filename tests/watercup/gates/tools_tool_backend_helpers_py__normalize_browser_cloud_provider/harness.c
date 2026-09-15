/* gate: tools_tool_backend_helpers_py__normalize_browser_cloud_provider — traced copy of tools_tool_backend_helpers_normalize_browser_cloud_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_tool_backend_helpers_normalize_browser_cloud_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_tool_backend_helpers_normalize_browser_cloud_provider");
    int result = tools_tool_backend_helpers_normalize_browser_cloud_provider(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_tool_backend_helpers_normalize_browser_cloud_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_tool_backend_helpers_normalize_browser_cloud_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
