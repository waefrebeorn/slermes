/* gate: hermes_cli_models_py___ai_gateway_model_is_free — traced copy of _ai_gateway_model_is_free */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool _ai_gateway_model_is_free(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_ai_gateway_model_is_free");
    bool result = _ai_gateway_model_is_free(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_ai_gateway_model_is_free");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _ai_gateway_model_is_free <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
