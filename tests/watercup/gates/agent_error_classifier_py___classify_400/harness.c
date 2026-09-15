/* gate: agent_error_classifier_py___classify_400 — traced copy of agent_error_classifier_u_classify_400 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_error_classifier_u_classify_400(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_error_classifier_u_classify_400");
    int result = agent_error_classifier_u_classify_400(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_error_classifier_u_classify_400");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_error_classifier_u_classify_400 <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
