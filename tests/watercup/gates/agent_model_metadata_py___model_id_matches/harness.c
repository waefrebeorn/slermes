/* gate: agent_model_metadata_py___model_id_matches — traced copy of model_id_matches */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int model_id_matches(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("model_id_matches");
    int result = model_id_matches(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("model_id_matches");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model_id_matches <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
