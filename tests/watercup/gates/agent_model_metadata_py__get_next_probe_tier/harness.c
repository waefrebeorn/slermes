/* gate: agent_model_metadata_py__get_next_probe_tier — traced copy of get_next_probe_tier */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int get_next_probe_tier(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("get_next_probe_tier");
    int result = get_next_probe_tier(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("get_next_probe_tier");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s get_next_probe_tier <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
