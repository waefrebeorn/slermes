/* gate: tools_approval_py___iter_top_level_shell_segments — traced copy of appr_u_iter_top_level_shell_segments */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int appr_u_iter_top_level_shell_segments(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("appr_u_iter_top_level_shell_segments");
    int result = appr_u_iter_top_level_shell_segments(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("appr_u_iter_top_level_shell_segments");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s appr_u_iter_top_level_shell_segments <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
