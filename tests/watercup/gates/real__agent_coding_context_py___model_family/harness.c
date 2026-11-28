/* gate: _model_family(model) -> "patch" | "replace" | None. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
const char *coding_context_model_family(const char *model);
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_model_family");
        const char *f = coding_context_model_family(argv[i]);
        WCUP_VALSTR(f ? f : "None");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("_model_family");
    }
    return 0;
}
