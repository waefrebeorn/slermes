/* gate: tools_mcp_schema_cache_py__has_cached_entry — traced copy of mcp_has_cached_entry */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcp_has_cached_entry(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcp_has_cached_entry");
    int result = mcp_has_cached_entry(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcp_has_cached_entry");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcp_has_cached_entry <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
