/* gate: tools_skills_hub_py__remove — traced copy of hub_taps_remove */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool hub_taps_remove(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hub_taps_remove");
    bool result = hub_taps_remove(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hub_taps_remove");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hub_taps_remove <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
