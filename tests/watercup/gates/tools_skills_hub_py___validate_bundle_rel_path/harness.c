/* gate: tools_skills_hub_py___validate_bundle_rel_path — traced copy of hub_validate_bundle_rel_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool hub_validate_bundle_rel_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hub_validate_bundle_rel_path");
    bool result = hub_validate_bundle_rel_path(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hub_validate_bundle_rel_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hub_validate_bundle_rel_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
