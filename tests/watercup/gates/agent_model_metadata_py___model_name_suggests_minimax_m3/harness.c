/* gate: agent_model_metadata_py___model_name_suggests_minimax_m3 — traced copy of mmd_model_name_suggests_minimax_m3 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mmd_model_name_suggests_minimax_m3(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mmd_model_name_suggests_minimax_m3");
    bool result = mmd_model_name_suggests_minimax_m3(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mmd_model_name_suggests_minimax_m3");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mmd_model_name_suggests_minimax_m3 <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
