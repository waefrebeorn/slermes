/* gate: agent_error_classifier_py___is_openrouter_upstream_error — traced copy of cli_agent_error_classifier__is_openrouter_upstream_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_agent_error_classifier__is_openrouter_upstream_error(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cli_agent_error_classifier__is_openrouter_upstream_error");
    int result = cli_agent_error_classifier__is_openrouter_upstream_error(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_agent_error_classifier__is_openrouter_upstream_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_error_classifier__is_openrouter_upstream_error <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
