/* gate: tools_bot_mode_probe_py__is_bot_mode_managed — traced copy of bot_mode_probe_is_bot_mode_managed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool bot_mode_probe_is_bot_mode_managed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("bot_mode_probe_is_bot_mode_managed");
    bool result = bot_mode_probe_is_bot_mode_managed(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bot_mode_probe_is_bot_mode_managed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bot_mode_probe_is_bot_mode_managed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
