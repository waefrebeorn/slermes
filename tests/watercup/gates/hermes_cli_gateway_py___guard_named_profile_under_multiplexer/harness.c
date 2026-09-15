/* gate: hermes_cli_gateway_py___guard_named_profile_under_multiplexer — traced copy of gwc2_guard_named_profile_under_multiplexer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gwc2_guard_named_profile_under_multiplexer(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gwc2_guard_named_profile_under_multiplexer");
    bool result = gwc2_guard_named_profile_under_multiplexer(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gwc2_guard_named_profile_under_multiplexer");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gwc2_guard_named_profile_under_multiplexer <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
