/* gate: agent_model_metadata_py__is_grok_46_family — traced copy of is_grok_46_family */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_grok_46_family(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_grok_46_family");
    bool result = is_grok_46_family(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_grok_46_family");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_grok_46_family <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
