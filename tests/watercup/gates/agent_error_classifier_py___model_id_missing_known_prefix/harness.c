/* gate: agent_error_classifier_py___model_id_missing_known_prefix — traced copy of cli_agent_error_classifier_model_id_missing_known_prefix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_agent_error_classifier_model_id_missing_known_prefix(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cli_agent_error_classifier_model_id_missing_known_prefix");
    int result = cli_agent_error_classifier_model_id_missing_known_prefix(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_agent_error_classifier_model_id_missing_known_prefix");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_error_classifier_model_id_missing_known_prefix <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
