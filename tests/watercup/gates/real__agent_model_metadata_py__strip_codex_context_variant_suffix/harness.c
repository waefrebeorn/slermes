/* gate: real__agent_model_metadata_py__strip_codex_context_variant_suffix — GAP sentinel for strip_codex_context_variant_suffix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "strip_codex_context_variant_suffix";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s strip_codex_context_variant_suffix <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("strip_codex_context_variant_suffix");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("strip_codex_context_variant_suffix");
    }
    return 0;
}
