/* gate: agent_auxiliary_client_py__resolve_compression_fast_lane — traced copy of resolve_compression_fast_lane */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int resolve_compression_fast_lane(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("resolve_compression_fast_lane");
    int result = resolve_compression_fast_lane(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("resolve_compression_fast_lane");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_compression_fast_lane <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
