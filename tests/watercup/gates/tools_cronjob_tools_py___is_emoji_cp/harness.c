/* gate: tools_cronjob_tools_py___is_emoji_cp — traced copy of cp_is_emoji */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cp_is_emoji(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cp_is_emoji");
    int result = cp_is_emoji(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cp_is_emoji");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cp_is_emoji <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
