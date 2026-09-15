/* gate: tools_approval_py___grep_safe_detection_variant — traced copy of appr_u_grep_safe_detection_variant */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int appr_u_grep_safe_detection_variant(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("appr_u_grep_safe_detection_variant");
    int result = appr_u_grep_safe_detection_variant(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("appr_u_grep_safe_detection_variant");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s appr_u_grep_safe_detection_variant <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
