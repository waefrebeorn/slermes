/* gate: hermes_state_py___is_background_review_harness_message — traced copy of hermes_state_is_background_review_harness_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool hermes_state_is_background_review_harness_message(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_state_is_background_review_harness_message");
    bool result = hermes_state_is_background_review_harness_message(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_state_is_background_review_harness_message");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_state_is_background_review_harness_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
