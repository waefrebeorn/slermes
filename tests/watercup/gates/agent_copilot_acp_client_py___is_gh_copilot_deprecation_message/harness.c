/* gate: agent_copilot_acp_client_py___is_gh_copilot_deprecation_message — traced copy of cac_is_gh_copilot_deprecation_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cac_is_gh_copilot_deprecation_message(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cac_is_gh_copilot_deprecation_message");
    bool result = cac_is_gh_copilot_deprecation_message(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cac_is_gh_copilot_deprecation_message");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cac_is_gh_copilot_deprecation_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
