/* gate: hermes_cli_plugins_cmd_py___resolve_tool_override_grant — traced copy of pcmd_resolve_tool_override_grant */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pcmd_resolve_tool_override_grant(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pcmd_resolve_tool_override_grant");
    int result = pcmd_resolve_tool_override_grant(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pcmd_resolve_tool_override_grant");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pcmd_resolve_tool_override_grant <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
