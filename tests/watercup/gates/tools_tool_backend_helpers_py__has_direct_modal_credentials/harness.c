/* gate: tools_tool_backend_helpers_py__has_direct_modal_credentials — traced copy of tools_tool_backend_helpers_has_direct_modal_credentials */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_tool_backend_helpers_has_direct_modal_credentials(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_tool_backend_helpers_has_direct_modal_credentials");
    int result = tools_tool_backend_helpers_has_direct_modal_credentials(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_tool_backend_helpers_has_direct_modal_credentials");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_tool_backend_helpers_has_direct_modal_credentials <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
