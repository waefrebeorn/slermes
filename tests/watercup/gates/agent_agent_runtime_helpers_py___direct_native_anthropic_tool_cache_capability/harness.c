/* gate: agent_agent_runtime_helpers_py___direct_native_anthropic_tool_cache_capability — traced copy of arh_direct_native_anthropic_tool_cache_capability */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int arh_direct_native_anthropic_tool_cache_capability(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("arh_direct_native_anthropic_tool_cache_capability");
    int result = arh_direct_native_anthropic_tool_cache_capability(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("arh_direct_native_anthropic_tool_cache_capability");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s arh_direct_native_anthropic_tool_cache_capability <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
