/* gate: agent_error_classifier_py___extract_status_code — traced copy of cli_agent_error_classifier__extract_status_code */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_agent_error_classifier__extract_status_code(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_agent_error_classifier__extract_status_code");
    int result = cli_agent_error_classifier__extract_status_code(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_agent_error_classifier__extract_status_code");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_error_classifier__extract_status_code <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
