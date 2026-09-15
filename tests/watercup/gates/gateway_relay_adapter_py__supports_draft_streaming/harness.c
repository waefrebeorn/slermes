/* gate: gateway_relay_adapter_py__supports_draft_streaming — traced copy of rla_supports_draft_streaming */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool rla_supports_draft_streaming(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("rla_supports_draft_streaming");
    bool result = rla_supports_draft_streaming(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("rla_supports_draft_streaming");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rla_supports_draft_streaming <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
