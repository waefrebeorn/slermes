/* gate: agent_gemini_native_adapter_py__is_free_tier_quota_error — traced copy of gna_is_free_tier_quota_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gna_is_free_tier_quota_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gna_is_free_tier_quota_error");
    bool result = gna_is_free_tier_quota_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gna_is_free_tier_quota_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gna_is_free_tier_quota_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
