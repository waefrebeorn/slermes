/* gate: agent_context_compressor_py___fallback_to_main_for_compression — traced copy of cli_agent_context_compressor__fallback_to_main_for_compression */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_agent_context_compressor__fallback_to_main_for_compression(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cli_agent_context_compressor__fallback_to_main_for_compression");
    int result = cli_agent_context_compressor__fallback_to_main_for_compression(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_agent_context_compressor__fallback_to_main_for_compression");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_context_compressor__fallback_to_main_for_compression <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
