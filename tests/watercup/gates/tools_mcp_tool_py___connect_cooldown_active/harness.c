/* gate: tools_mcp_tool_py___connect_cooldown_active — traced copy of mcp_connect_cooldown_active */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mcp_connect_cooldown_active(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_connect_cooldown_active");
    bool result = mcp_connect_cooldown_active(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_connect_cooldown_active");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_connect_cooldown_active <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
