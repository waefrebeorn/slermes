/* gate: agent_file_safety_py__classify_cross_profile_target — traced copy of fsf_classify_cross_profile_target */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool fsf_classify_cross_profile_target(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("fsf_classify_cross_profile_target");
    bool result = fsf_classify_cross_profile_target(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fsf_classify_cross_profile_target");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fsf_classify_cross_profile_target <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
