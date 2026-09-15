/* gate: tools_skill_usage_py__is_curator_managed — traced copy of su_is_curator_managed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int su_is_curator_managed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("su_is_curator_managed");
    int result = su_is_curator_managed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("su_is_curator_managed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s su_is_curator_managed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
