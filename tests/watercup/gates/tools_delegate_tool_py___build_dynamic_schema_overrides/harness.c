/* gate: tools_delegate_tool_py___build_dynamic_schema_overrides — traced copy of tools_delegate_tool_u_build_dynamic_schema_overrides */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_delegate_tool_u_build_dynamic_schema_overrides(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_delegate_tool_u_build_dynamic_schema_overrides");
    int result = tools_delegate_tool_u_build_dynamic_schema_overrides(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_delegate_tool_u_build_dynamic_schema_overrides");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_delegate_tool_u_build_dynamic_schema_overrides <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
