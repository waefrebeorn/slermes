/* gate: agent_anthropic_adapter_py___model_name_is_kimi_family — traced copy of antr_model_name_is_kimi_family */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool antr_model_name_is_kimi_family(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("antr_model_name_is_kimi_family");
    bool result = antr_model_name_is_kimi_family(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("antr_model_name_is_kimi_family");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s antr_model_name_is_kimi_family <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
