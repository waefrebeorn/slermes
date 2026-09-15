/* gate: tools_computer_use_vision_routing_py___explicit_aux_vision_override — traced copy of tools_vision_aux_override */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool tools_vision_aux_override(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_vision_aux_override");
    bool result = tools_vision_aux_override(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_vision_aux_override");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_vision_aux_override <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
